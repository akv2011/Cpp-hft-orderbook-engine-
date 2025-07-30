#!/usr/bin/env python3
"""
Analyze action patterns in MBO vs MBP data to understand snapshot generation logic
"""

import csv
from collections import defaultdict, Counter

def read_mbo_data(filename):
    """Read MBO events from CSV file"""
    events = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Handle empty/missing values
            price = float(row['price']) if row['price'].strip() else 0.0
            size = int(row['size']) if row['size'].strip() else 0
            order_id = int(row['order_id']) if row['order_id'].strip() else 0
            
            events.append({
                'ts_event': row['ts_event'],
                'action': row['action'],
                'side': row['side'],
                'price': price,
                'size': size,
                'order_id': order_id
            })
    return events

def read_mbp_data(filename):
    """Read MBP snapshots from CSV file"""
    snapshots = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            snapshots.append({
                'ts_event': row['ts_event'],
                'action': row['action'],
                'side': row['side']
            })
    return snapshots

def analyze_action_distribution():
    """Analyze action distribution in both MBO and MBP data"""
    print("Reading MBO data from quant_dev_trial/mbo.csv...")
    mbo_events = read_mbo_data('quant_dev_trial/mbo.csv')
    
    print("Reading reference MBP data from quant_dev_trial/mbp.csv...")
    mbp_snapshots = read_mbp_data('quant_dev_trial/mbp.csv')
    
    print(f"MBO events: {len(mbo_events)}")
    print(f"MBP snapshots: {len(mbp_snapshots)}")
    
    # Count actions in MBO
    mbo_actions = Counter([event['action'] for event in mbo_events])
    print(f"\nMBO Action Distribution:")
    for action, count in sorted(mbo_actions.items()):
        print(f"  {action}: {count}")
    
    # Count actions in MBP
    mbp_actions = Counter([snap['action'] for snap in mbp_snapshots])
    print(f"\nMBP Action Distribution:")
    for action, count in sorted(mbp_actions.items()):
        print(f"  {action}: {count}")
    
    # Calculate ratios
    print(f"\nAction Ratios (MBP/MBO):")
    for action in sorted(set(mbo_actions.keys()).union(set(mbp_actions.keys()))):
        mbo_count = mbo_actions.get(action, 0)
        mbp_count = mbp_actions.get(action, 0)
        if mbo_count > 0:
            ratio = mbp_count / mbo_count
            print(f"  {action}: {mbp_count}/{mbo_count} = {ratio:.3f}")
        else:
            print(f"  {action}: {mbp_count}/0 = N/A")

def analyze_snapshot_triggers():
    """Analyze what triggers snapshots in the reference data"""
    print("\n" + "="*50)
    print("SNAPSHOT TRIGGER ANALYSIS")
    print("="*50)
    
    mbo_events = read_mbo_data('quant_dev_trial/mbo.csv')
    mbp_snapshots = read_mbp_data('quant_dev_trial/mbp.csv')
    
    # Create timestamp to MBO event mapping
    mbo_by_timestamp = {}
    for event in mbo_events:
        mbo_by_timestamp[event['ts_event']] = event
    
    # Check which MBO events triggered snapshots
    triggered_actions = []
    not_triggered_actions = []
    
    snapshot_timestamps = set(snap['ts_event'] for snap in mbp_snapshots)
    
    for event in mbo_events:
        if event['ts_event'] in snapshot_timestamps:
            triggered_actions.append(event['action'])
        else:
            not_triggered_actions.append(event['action'])
    
    print(f"Events that triggered snapshots:")
    triggered_counter = Counter(triggered_actions)
    for action, count in sorted(triggered_counter.items()):
        print(f"  {action}: {count}")
    
    print(f"\nEvents that did NOT trigger snapshots:")
    not_triggered_counter = Counter(not_triggered_actions)
    for action, count in sorted(not_triggered_counter.items()):
        print(f"  {action}: {count}")
    
    # Calculate trigger rates
    print(f"\nTrigger rates by action:")
    all_actions = Counter([event['action'] for event in mbo_events])
    for action in sorted(all_actions.keys()):
        total = all_actions[action]
        triggered = triggered_counter.get(action, 0)
        rate = triggered / total if total > 0 else 0
        print(f"  {action}: {triggered}/{total} = {rate:.3f}")

if __name__ == "__main__":
    analyze_action_distribution()
    analyze_snapshot_triggers()
