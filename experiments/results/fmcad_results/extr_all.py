# this script expects exacly one parameter, namely a directory name
# All text files in this directory are expected to be performance
# result files and will be turned into csv files

import sys
import re
import os

script_name = "extr_learn_log_to_table.py"

dir_name = sys.argv[1]
text_files = [f for f in os.listdir(dir_name) if f.endswith('.txt')]

for filename in text_files:
    outfile = filename.replace(".txt", ".csv")
    print outfile
    os.system("python " + script_name + " " + filename + " " + outfile)

    