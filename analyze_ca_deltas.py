#!/usr/bin/env python3
"""
Analysis script to determine optimal time window for C->A consolidation
"""

import pandas as pd
import numpy as np
from datetime import datetime

def analyze_cancel_add_deltas(mbo_file):
    """Analyze time deltas between Cancel and Add events for same order_id"""
    
    # Read MBO data
    print(f"Reading MBO data from {mbo_file}...")
    df = pd.read_csv(mbo_file)
    
    # Convert timestamp to nanoseconds
    df['ts_event_ns'] = pd.to_datetime(df['ts_event']).astype('int64')
    
    print(f"Total events: {len(df)}")
    print(f"Cancel events: {len(df[df['action'] == 'C'])}")
    print(f"Add events: {len(df[df['action'] == 'A'])}")
    
    # Find potential C->A pairs
    cancel_events = df[df['action'] == 'C'].copy()
    add_events = df[df['action'] == 'A'].copy()
    
    print(f"\nAnalyzing potential C->A consolidation pairs...")
    
    deltas = []
    consolidation_candidates = []
    
    for _, cancel in cancel_events.iterrows():
        # Find Add events for the same order_id that come after this Cancel
        matching_adds = add_events[
            (add_events['order_id'] == cancel['order_id']) & 
            (add_events['ts_event_ns'] > cancel['ts_event_ns'])
        ].copy()
        
        if not matching_adds.empty:
            # Take the first (earliest) matching Add
            next_add = matching_adds.iloc[0]
            delta_ns = next_add['ts_event_ns'] - cancel['ts_event_ns']
            deltas.append(delta_ns)
            
            consolidation_candidates.append({
                'order_id': cancel['order_id'],
                'cancel_time': cancel['ts_event_ns'],
                'add_time': next_add['ts_event_ns'],
                'delta_ns': delta_ns,
                'delta_us': delta_ns / 1000,
                'delta_ms': delta_ns / 1_000_000
            })
    
    if not deltas:
        print("No C->A pairs found!")
        return
    
    deltas = np.array(deltas)
    
    print(f"\nFound {len(deltas)} potential C->A consolidation pairs")
    print(f"Time delta statistics (nanoseconds):")
    print(f"  Min: {deltas.min():,} ns ({deltas.min()/1000:.1f} μs)")
    print(f"  Max: {deltas.max():,} ns ({deltas.max()/1_000_000:.1f} ms)")
    print(f"  Mean: {deltas.mean():.0f} ns ({deltas.mean()/1000:.1f} μs)")
    print(f"  Median: {np.median(deltas):.0f} ns ({np.median(deltas)/1000:.1f} μs)")
    print(f"  25th percentile: {np.percentile(deltas, 25):.0f} ns ({np.percentile(deltas, 25)/1000:.1f} μs)")
    print(f"  75th percentile: {np.percentile(deltas, 75):.0f} ns ({np.percentile(deltas, 75)/1000:.1f} μs)")
    print(f"  95th percentile: {np.percentile(deltas, 95):.0f} ns ({np.percentile(deltas, 95)/1000:.1f} μs)")
    print(f"  99th percentile: {np.percentile(deltas, 99):.0f} ns ({np.percentile(deltas, 99)/1000:.1f} μs)")
    
    # Show sample candidates
    print(f"\nFirst 10 consolidation candidates:")
    candidates_df = pd.DataFrame(consolidation_candidates[:10])
    print(candidates_df[['order_id', 'delta_ns', 'delta_us', 'delta_ms']].to_string())
    
    # Suggest optimal time windows
    print(f"\nTime window recommendations:")
    for percentile in [50, 75, 90, 95, 99]:
        threshold_ns = np.percentile(deltas, percentile)
        count_within = np.sum(deltas <= threshold_ns)
        coverage = (count_within / len(deltas)) * 100
        print(f"  {threshold_ns:.0f} ns ({threshold_ns/1000:.1f} μs): captures {count_within}/{len(deltas)} pairs ({coverage:.1f}%)")

if __name__ == "__main__":
    analyze_cancel_add_deltas("quant_dev_trial/mbo.csv")
