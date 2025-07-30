#!/usr/bin/env python3
"""
Advanced MBO Discrepancy Analysis
Focused analysis to understand the remaining 369 snapshot discrepancy

Current Status:
- Target: 3,929 snapshots
- Current: 4,298 snapshots  
- Gap: 369 snapshots

This script investigates patterns that might explain the remaining discrepancy.
"""

import pandas as pd
import numpy as np
from collections import defaultdict, Counter
from datetime import datetime, timedelta

class DiscrepancyAnalyzer:
    def __init__(self, mbo_file, expected_file=None):
        """Initialize analyzer with MBO data and optional expected output"""
        self.mbo_file = mbo_file
        self.expected_file = expected_file
        self.df = pd.read_csv(mbo_file)
        self.df['ts_parsed'] = pd.to_datetime(self.df['ts_event'])
        print(f"Loaded {len(self.df)} MBO events")
    
    def analyze_cancel_add_patterns(self):
        """Analyze potential Cancel-Add patterns beyond the current implementation"""
        print("\n" + "="*60)
        print("🔍 ADVANCED CANCEL-ADD PATTERN ANALYSIS")
        print("="*60)
        
        # Group by timestamp to find more sophisticated patterns
        timestamp_groups = self.df.groupby('ts_event')
        
        # Look for patterns within microsecond windows
        microsecond_patterns = []
        rapid_fire_patterns = []
        
        for timestamp, group in timestamp_groups:
            if len(group) > 2:  # Groups with multiple events
                actions = group['action'].tolist()
                sides = group['side'].tolist()
                
                # Count different patterns
                c_count = actions.count('C')
                a_count = actions.count('A')
                
                if c_count > 0 and a_count > 0:
                    pattern_info = {
                        'timestamp': timestamp,
                        'total_events': len(group),
                        'cancels': c_count,
                        'adds': a_count,
                        'actions': actions,
                        'sides': sides,
                        'prices': group['price'].tolist(),
                        'sizes': group['size'].tolist()
                    }
                    
                    if c_count == a_count and len(group) == c_count + a_count:
                        # Pure Cancel-Add replacement pattern
                        microsecond_patterns.append(pattern_info)
                    elif len(group) > 10:  # High-frequency trading bursts
                        rapid_fire_patterns.append(pattern_info)
        
        print(f"Microsecond Cancel-Add Patterns: {len(microsecond_patterns)}")
        print(f"Rapid-Fire Trading Patterns: {len(rapid_fire_patterns)}")
        
        # Analyze existing Cancel-Add detection effectiveness
        if microsecond_patterns:
            print("\nSample Microsecond Patterns:")
            for i, pattern in enumerate(microsecond_patterns[:3]):
                print(f"  {i+1}. Time: {pattern['timestamp']}")
                print(f"     Events: {pattern['total_events']} (C:{pattern['cancels']}, A:{pattern['adds']})")
                print(f"     Actions: {pattern['actions']}")
        
        return microsecond_patterns, rapid_fire_patterns
    
    def analyze_order_lifecycle(self):
        """Analyze complete order lifecycles to find consolidation opportunities"""
        print("\n" + "="*60)
        print("📈 ORDER LIFECYCLE ANALYSIS")
        print("="*60)
        
        # Track orders from Add to Cancel
        order_lifecycles = defaultdict(list)
        
        for _, row in self.df.iterrows():
            if row['action'] in ['A', 'C', 'M'] and row['order_id'] > 0:
                order_lifecycles[row['order_id']].append({
                    'timestamp': row['ts_event'],
                    'action': row['action'],
                    'side': row['side'],
                    'price': row['price'],
                    'size': row['size']
                })
        
        # Analyze lifecycle patterns
        short_lived_orders = []  # Orders that exist for very short time
        rapid_modifications = []  # Orders with rapid modifications
        
        for order_id, lifecycle in order_lifecycles.items():
            if len(lifecycle) >= 2:
                # Sort by timestamp
                lifecycle_sorted = sorted(lifecycle, key=lambda x: x['timestamp'])
                
                # Check for short-lived orders (Add immediately followed by Cancel)
                if (len(lifecycle_sorted) == 2 and 
                    lifecycle_sorted[0]['action'] == 'A' and 
                    lifecycle_sorted[1]['action'] == 'C'):
                    
                    # Calculate time difference
                    t1 = pd.to_datetime(lifecycle_sorted[0]['timestamp'])
                    t2 = pd.to_datetime(lifecycle_sorted[1]['timestamp'])
                    time_diff = (t2 - t1).total_seconds()
                    
                    short_lived_orders.append({
                        'order_id': order_id,
                        'time_alive': time_diff,
                        'add_time': lifecycle_sorted[0]['timestamp'],
                        'cancel_time': lifecycle_sorted[1]['timestamp'],
                        'side': lifecycle_sorted[0]['side'],
                        'price': lifecycle_sorted[0]['price'],
                        'size': lifecycle_sorted[0]['size']
                    })
                
                elif len(lifecycle_sorted) > 2:
                    rapid_modifications.append({
                        'order_id': order_id,
                        'modification_count': len(lifecycle_sorted),
                        'lifecycle': lifecycle_sorted
                    })
        
        print(f"Short-lived Orders (Add->Cancel): {len(short_lived_orders)}")
        print(f"Orders with Multiple Modifications: {len(rapid_modifications)}")
        
        if short_lived_orders:
            # Analyze short-lived order timing
            time_alive_list = [order['time_alive'] for order in short_lived_orders]
            avg_time_alive = np.mean(time_alive_list)
            
            print(f"\nShort-lived Order Statistics:")
            print(f"  Average time alive: {avg_time_alive:.6f} seconds")
            print(f"  Orders alive < 1ms: {sum(1 for t in time_alive_list if t < 0.001)}")
            print(f"  Orders alive < 1μs: {sum(1 for t in time_alive_list if t < 0.000001)}")
            
            # Show samples
            print("\nSample Short-lived Orders:")
            for i, order in enumerate(short_lived_orders[:5]):
                print(f"  {i+1}. Order {order['order_id']}: "
                      f"{order['time_alive']:.6f}s alive, "
                      f"Side: {order['side']}, "
                      f"Price: ${order['price']:.2f}")
        
        return short_lived_orders, rapid_modifications
    
    def analyze_price_level_consolidation(self):
        """Analyze if multiple events at same price levels should be consolidated"""
        print("\n" + "="*60)
        print("💰 PRICE LEVEL CONSOLIDATION ANALYSIS")
        print("="*60)
        
        # Group events by price and timestamp windows
        price_groups = defaultdict(list)
        
        for _, row in self.df.iterrows():
            if not pd.isna(row['price']) and row['price'] > 0:
                # Round timestamp to nearest millisecond for grouping
                ts = pd.to_datetime(row['ts_event'])
                ts_ms = ts.round('1ms')
                
                key = (row['price'], ts_ms, row['side'])
                price_groups[key].append(row)
        
        # Find price levels with multiple simultaneous events
        multi_event_levels = []
        for key, events in price_groups.items():
            if len(events) > 1:
                price, timestamp, side = key
                multi_event_levels.append({
                    'price': price,
                    'timestamp': timestamp,
                    'side': side,
                    'event_count': len(events),
                    'actions': [e['action'] for e in events],
                    'total_size': sum(e['size'] for e in events if e['size'] > 0)
                })
        
        print(f"Price levels with multiple simultaneous events: {len(multi_event_levels)}")
        
        if multi_event_levels:
            # Analyze patterns
            action_patterns = Counter()
            for level in multi_event_levels:
                pattern = ','.join(sorted(level['actions']))
                action_patterns[pattern] += 1
            
            print("\nAction patterns at same price/time:")
            for pattern, count in action_patterns.most_common(10):
                print(f"  {pattern}: {count} occurrences")
            
            print("\nSample multi-event price levels:")
            for i, level in enumerate(multi_event_levels[:5]):
                print(f"  {i+1}. ${level['price']:.2f} at {level['timestamp']}")
                print(f"     Side: {level['side']}, Events: {level['event_count']}")
                print(f"     Actions: {level['actions']}")
        
        return multi_event_levels
    
    def estimate_consolidation_potential(self):
        """Estimate how many events could potentially be consolidated"""
        print("\n" + "="*60)
        print("🎯 CONSOLIDATION POTENTIAL ESTIMATION")
        print("="*60)
        
        total_events = len(self.df)
        current_output = 4298  # From our current implementation
        target_output = 3929
        gap = current_output - target_output
        
        print(f"Current Analysis:")
        print(f"  Total MBO events: {total_events}")
        print(f"  Current output snapshots: {current_output}")
        print(f"  Target output snapshots: {target_output}")
        print(f"  Remaining gap: {gap}")
        
        # Analyze by action type
        actions = self.df['action'].value_counts()
        print(f"\nAction breakdown:")
        for action, count in actions.items():
            print(f"  {action}: {count}")
        
        # Calculate theoretical minimum snapshots
        # Assuming perfect consolidation
        add_orders = actions.get('A', 0)
        cancel_orders = actions.get('C', 0)
        trades = actions.get('T', 0)
        
        # Each unique Add that isn't immediately cancelled = 1 snapshot
        # Each unique Cancel that isn't part of replacement = 1 snapshot  
        # Each Trade = 1 snapshot (after T->F->C consolidation)
        
        print(f"\nTheoretical analysis:")
        print(f"  If every Add creates a snapshot: {add_orders}")
        print(f"  If every Cancel creates a snapshot: {cancel_orders}")
        print(f"  Trades (after consolidation): {trades}")
        print(f"  Maximum possible snapshots: {add_orders + cancel_orders + trades}")
        print(f"  Current reduction needed: {gap} snapshots")
        print(f"  Percentage reduction needed: {(gap/current_output)*100:.1f}%")
        
        # Look for rapid succession events (likely candidates for consolidation)
        rapid_events = 0
        prev_time = None
        
        df_sorted = self.df.sort_values('ts_parsed')
        for _, row in df_sorted.iterrows():
            if prev_time is not None:
                time_diff = (row['ts_parsed'] - prev_time).total_seconds()
                if time_diff < 0.001:  # Less than 1 millisecond
                    rapid_events += 1
            prev_time = row['ts_parsed']
        
        print(f"\nTiming analysis:")
        print(f"  Events within 1ms of previous: {rapid_events}")
        print(f"  Potential for timing-based consolidation: {rapid_events} events")
    
    def run_analysis(self):
        """Run the complete discrepancy analysis"""
        print("🔍 ADVANCED DISCREPANCY ANALYSIS")
        print("="*80)
        print("Investigating the remaining 369 snapshot discrepancy...")
        
        patterns1, patterns2 = self.analyze_cancel_add_patterns()
        short_orders, rapid_mods = self.analyze_order_lifecycle()
        multi_levels = self.analyze_price_level_consolidation()
        self.estimate_consolidation_potential()
        
        print("\n" + "="*80)
        print("📊 ANALYSIS SUMMARY")
        print("="*80)
        print("Key findings for the 369-snapshot discrepancy:")
        print(f"  1. Microsecond Cancel-Add patterns: {len(patterns1)} groups")
        print(f"  2. Short-lived orders (Add->Cancel): {len(short_orders)} orders")
        print(f"  3. Multi-event price levels: {len(multi_levels)} levels")
        print(f"  4. Orders with rapid modifications: {len(rapid_mods)} orders")
        
        print("\nRecommendations:")
        if len(short_orders) > 300:
            print("  • Implement short-lived order consolidation (Add->Cancel < 1ms)")
        if len(patterns1) > 50:
            print("  • Enhance Cancel-Add pair detection for microsecond patterns")
        if len(multi_levels) > 100:
            print("  • Consider price-level event aggregation")
        
        print("\n✅ Advanced analysis complete!")

def main():
    """Run the discrepancy analysis"""
    mbo_file = r"c:\Users\arunk\Cpp-hft-orderbook-engine-\quant_dev_trial\mbo.csv"
    
    analyzer = DiscrepancyAnalyzer(mbo_file)
    analyzer.run_analysis()

if __name__ == "__main__":
    main()
