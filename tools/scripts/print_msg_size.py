#!/usr/bin/env python3
import argparse
from tqdm import tqdm
from collections import defaultdict

from openpilot.tools.lib.logreader import LogReader

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("route", help="The route name")
  args = parser.parse_args()

  lr = LogReader(args.route)

  # Dictionaries to store total size and message count for each message type
  total_size = defaultdict(int)
  msg_count = defaultdict(int)

  # Process each message
  for msg in tqdm(lr):
    msg_type = msg.which()  # Get the type of message
    msg_size = len(msg.as_builder().to_bytes())  # Get the message size in bytes

    total_size[msg_type] += msg_size  # Add the size to the total
    msg_count[msg_type] += 1  # Increment the count for this message type

  # Calculate average size for each message type and store it in a dictionary
  avg_size = {msg_type: total_size[msg_type] / msg_count[msg_type] for msg_type in total_size}

  # Sort the dictionary by average message size in ascending order
  sorted_avg_size = sorted(avg_size.items(), key=lambda x: x[1])

  # Print the results as a table
  print(f"{'Message Type':<25}{'Average Size (bytes)':>20}")
  print("-" * 45)
  for msg_type, size in sorted_avg_size:
    print(f"{msg_type:<25}{size:>20.2f}")
