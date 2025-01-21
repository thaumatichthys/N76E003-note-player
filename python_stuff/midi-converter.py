import mido
import numpy as np
import os.path


# note: this code isnt functional, it fucks up the time base.

# ; PROTOCOL: DATA:
# ; 2 bytes for counter value, 2 bytes for delay after execution, 4 bits of metadata, 4 bits of channel
# ; example: 0x1A, 0x02, 0x12, 0x34, 0x2
# ;
# ; metadata:   0000_0000 --> note off;
# ;             0001_0000 --> note on;
# ;             0011_0000 --> end of data


# --------- CONFIG START --------- #
midi_input_path = "../midi_files/pirates.mid"
text_output_path = "../music.inc"

# the arduino has really limited flash, so this option lets you cut off the rest of the track
max_output_size_bytes = 5000

BASE_FREQ = 440  # reference freq
SAMPLE_RATE = 16384
TICK_DIVIDER = 64  # the sample rate bitmask for each tick
FREQ_COEFF = int(65536 / SAMPLE_RATE)  # 2^16 / SAMPLE_RATE for the 8051 program
N_CHANNELS = 8

tick_rate = SAMPLE_RATE / TICK_DIVIDER  # in Hz
# ---------- CONFIG END ---------- #

mid = mido.MidiFile(midi_input_path)

output_array = []
output_length = 0
length = 0

prev_msg = 0
msg_num = 0

channels_active = np.zeros((N_CHANNELS))
channels_active -= 1

# channel picker: allocate and move up
# turning off: go through dictionary and find which channel is allocated to key [note]

output_str = "cseg \n\nmusic_data: db "

# get the midi data
for msg in mid:
    if (msg.type == 'note_on' or msg.type == 'note_off') and msg_num > 0:

        using_channel = -1

        metadata = 0
        if (prev_msg.type == 'note_on' or prev_msg.type == 'note_off'):
            if prev_msg.type == 'note_on' and prev_msg.velocity > 0:
                # find a channel and use it
                for i in range(N_CHANNELS):
                    if channels_active[i] == -1:
                        # found one
                        channels_active[i] = prev_msg.note
                        using_channel = i
                        metadata = 0b00010000
                        break
            else:
                for i in range(N_CHANNELS):
                    if channels_active[i] == prev_msg.note:
                        # found the channel, removing
                        channels_active[i] = -1
                        using_channel = i
                        metadata = 0b00000000
                        break

            # if for some reason the channel is not found, ignore the command
            if not (using_channel == -1):
                # use data from prev_msg, but time from current msg
                freq = round(BASE_FREQ * 2 ** ((prev_msg.note - 69) / 12))
                freq *= FREQ_COEFF
                time_converted = int(round(msg.time * tick_rate))

                byte0 = (freq & 0xFF)
                byte1 = ((freq >> 8) & 0xFF)
                byte2 = (time_converted & 0xFF)
                byte3 = ((time_converted >> 8) & 0xFF)

                print(time_converted)
                print(freq)

                byte4 = (using_channel & 0x0F) | (metadata & 0xF0)

                data = [byte0, byte1, byte2, byte3, byte4]

                for d in data:
                    output_str += str(hex(d)) + ", "
                output_length += 5
                if output_length >= max_output_size_bytes:
                    break

    prev_msg = msg
    msg_num += 1

output_str += "0, 0, 0, 0, 48\n"

with open(text_output_path, 'w') as f:
    f.write(output_str)

output_str += "\n// Data size: " + str(int(length * 4 / 100) / 10) + "kb (approx)\n"
print(output_str)