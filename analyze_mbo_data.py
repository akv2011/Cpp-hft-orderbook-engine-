#!/usr/bin/env python3
"""
MBO Data Analysis Script
Comprehensive analysis of Market-by-Order (MBO) CSV data

This script analyzes the MBO data to understand:
1. Types of actions/trades recorded
2. Data distribution and patterns
3. Trading behavior and sequences
4. Special cases and edge conditions
"""

import pandas as pd
import numpy as np
from collections import Counter, defaultdict
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

class MBOAnalyzer:
    def __init__(self, csv_file_path):
        """Initialize the analyzer with MBO CSV data"""
        self.csv_file = csv_file_path
        self.df = None
        self.load_data()
    
    def load_data(self):
        """Load and prepare the MBO data"""
        print(f"Loading MBO data from: {self.csv_file}")
        try:
            self.df = pd.read_csv(self.csv_file)
            print(f"✅ Successfully loaded {len(self.df)} records")
            print(f"📊 Data shape: {self.df.shape}")
            print(f"📝 Columns: {list(self.df.columns)}")
        except Exception as e:
            print(f"❌ Error loading data: {e}")
            return False
        return True
    
    def basic_info(self):
        """Display basic information about the dataset"""
        print("\n" + "="*80)
        print("📈 BASIC DATASET INFORMATION")
        print("="*80)
        
        print(f"Total Records: {len(self.df):,}")
        print(f"Date Range: {self.df['ts_event'].min()} to {self.df['ts_event'].max()}")
        print(f"Symbols: {self.df['symbol'].unique()}")
        print(f"Instrument IDs: {self.df['instrument_id'].unique()}")
        print(f"Publishers: {self.df['publisher_id'].unique()}")
        
        print("\nData Types:")
        print(self.df.dtypes)
        
        print("\nMemory Usage:")
        print(self.df.memory_usage(deep=True))
    
    def analyze_actions(self):
        """Analyze different types of actions in the MBO data"""
        print("\n" + "="*80)
        print("⚡ ACTION ANALYSIS")
        print("="*80)
        
        action_counts = self.df['action'].value_counts()
        print("Action Distribution:")
        for action, count in action_counts.items():
            percentage = (count / len(self.df)) * 100
            print(f"  {action}: {count:,} ({percentage:.2f}%)")
        
        # Action definitions based on Databento documentation
        action_meanings = {
            'A': 'Add order to book',
            'C': 'Cancel order from book', 
            'M': 'Modify existing order',
            'T': 'Trade executed',
            'F': 'Fill (partial execution)',
            'R': 'Reset/Clear book'
        }
        
        print("\nAction Meanings:")
        for action in action_counts.index:
            meaning = action_meanings.get(action, 'Unknown action type')
            print(f"  {action}: {meaning}")
        
        return action_counts
    
    def analyze_sides(self):
        """Analyze bid/ask sides and special cases"""
        print("\n" + "="*80)
        print("📊 SIDE ANALYSIS")
        print("="*80)
        
        side_counts = self.df['side'].value_counts()
        print("Side Distribution:")
        for side, count in side_counts.items():
            percentage = (count / len(self.df)) * 100
            side_meaning = {
                'A': 'Ask (Sell orders)',
                'B': 'Bid (Buy orders)',
                'N': 'None/Neutral (Special trades)'
            }.get(side, 'Unknown side')
            print(f"  {side}: {count:,} ({percentage:.2f}%) - {side_meaning}")
        
        # Analyze side by action
        print("\nSide Distribution by Action:")
        side_action = pd.crosstab(self.df['action'], self.df['side'], margins=True)
        print(side_action)
        
        return side_counts
    
    def analyze_prices_and_sizes(self):
        """Analyze price and size distributions"""
        print("\n" + "="*80)
        print("💰 PRICE & SIZE ANALYSIS")
        print("="*80)
        
        # Remove rows where price is NaN or 0 for analysis
        price_data = self.df[self.df['price'].notna() & (self.df['price'] > 0)]
        size_data = self.df[self.df['size'].notna() & (self.df['size'] > 0)]
        
        print("Price Statistics:")
        print(f"  Records with prices: {len(price_data):,}")
        print(f"  Price range: ${price_data['price'].min():.2f} - ${price_data['price'].max():.2f}")
        print(f"  Average price: ${price_data['price'].mean():.2f}")
        print(f"  Median price: ${price_data['price'].median():.2f}")
        print(f"  Price std dev: ${price_data['price'].std():.2f}")
        
        print("\nSize Statistics:")
        print(f"  Records with sizes: {len(size_data):,}")
        print(f"  Size range: {size_data['size'].min():,} - {size_data['size'].max():,}")
        print(f"  Average size: {size_data['size'].mean():.0f}")
        print(f"  Median size: {size_data['size'].median():.0f}")
        print(f"  Size std dev: {size_data['size'].std():.0f}")
        
        # Price levels analysis
        print("\nMost Common Price Levels:")
        price_counts = price_data['price'].value_counts().head(10)
        for price, count in price_counts.items():
            print(f"  ${price:.2f}: {count} orders")
        
        print("\nMost Common Order Sizes:")
        size_counts = size_data['size'].value_counts().head(10)
        for size, count in size_counts.items():
            print(f"  {size:,} shares: {count} orders")
    
    def analyze_trade_sequences(self):
        """Analyze T->F->C sequences as mentioned in the task description"""
        print("\n" + "="*80)
        print("🔄 TRADE SEQUENCE ANALYSIS (T->F->C)")
        print("="*80)
        
        # Convert timestamps for sequence analysis
        self.df['ts_parsed'] = pd.to_datetime(self.df['ts_event'])
        
        # Group by timestamp to find T->F->C sequences
        timestamp_groups = self.df.groupby('ts_event')
        
        tfc_sequences = []
        trade_only_events = []
        
        for timestamp, group in timestamp_groups:
            actions = group['action'].tolist()
            
            # Look for T->F->C patterns
            if 'T' in actions and 'F' in actions and 'C' in actions:
                # Check if it's a proper sequence
                t_events = group[group['action'] == 'T']
                f_events = group[group['action'] == 'F'] 
                c_events = group[group['action'] == 'C']
                
                # Try to match T->F->C by price and size
                for _, t_row in t_events.iterrows():
                    matching_f = f_events[
                        (f_events['price'] == t_row['price']) & 
                        (f_events['size'] == t_row['size'])
                    ]
                    
                    if not matching_f.empty:
                        f_row = matching_f.iloc[0]
                        # Look for corresponding cancel
                        matching_c = c_events[c_events['order_id'] == f_row['order_id']]
                        
                        if not matching_c.empty:
                            c_row = matching_c.iloc[0]
                            tfc_sequences.append({
                                'timestamp': timestamp,
                                't_side': t_row['side'],
                                'f_side': f_row['side'],
                                'c_side': c_row['side'],
                                'price': t_row['price'],
                                'size': t_row['size'],
                                'order_id': f_row['order_id']
                            })
            
            # Look for standalone T events
            elif 'T' in actions and len(group) == 1:
                t_row = group.iloc[0]
                trade_only_events.append({
                    'timestamp': timestamp,
                    'side': t_row['side'],
                    'price': t_row['price'],
                    'size': t_row['size']
                })
        
        print(f"T->F->C Sequences Found: {len(tfc_sequences)}")
        print(f"Standalone T Events: {len(trade_only_events)}")
        
        if tfc_sequences:
            print("\nSample T->F->C Sequences:")
            for i, seq in enumerate(tfc_sequences[:5]):
                print(f"  {i+1}. Time: {seq['timestamp']}")
                print(f"     Price: ${seq['price']:.2f}, Size: {seq['size']}")
                print(f"     Sides: T({seq['t_side']}) -> F({seq['f_side']}) -> C({seq['c_side']})")
        
        if trade_only_events:
            print("\nSample Standalone T Events:")
            for i, trade in enumerate(trade_only_events[:5]):
                print(f"  {i+1}. Time: {trade['timestamp']}")
                print(f"     Side: {trade['side']}, Price: ${trade['price']:.2f}, Size: {trade['size']}")
        
        return tfc_sequences, trade_only_events
    
    def analyze_special_cases(self):
        """Analyze special cases mentioned in the task"""
        print("\n" + "="*80)
        print("🔍 SPECIAL CASES ANALYSIS")
        print("="*80)
        
        # 1. Initial R action
        reset_actions = self.df[self.df['action'] == 'R']
        print(f"Reset (R) Actions: {len(reset_actions)}")
        if len(reset_actions) > 0:
            print("First Reset Action:")
            print(reset_actions.iloc[0].to_dict())
        
        # 2. T actions with side 'N'
        t_neutral = self.df[(self.df['action'] == 'T') & (self.df['side'] == 'N')]
        print(f"\nTrade (T) Actions with Side 'N': {len(t_neutral)}")
        if len(t_neutral) > 0:
            print("Sample T,N Actions:")
            for i, (_, row) in enumerate(t_neutral.head(3).iterrows()):
                print(f"  {i+1}. Price: ${row['price']:.2f}, Size: {row['size']}, Time: {row['ts_event']}")
        
        # 3. Orders without prices (likely cancellations or special actions)
        no_price = self.df[self.df['price'].isna() | (self.df['price'] == 0)]
        print(f"\nActions without Price: {len(no_price)}")
        if len(no_price) > 0:
            no_price_actions = no_price['action'].value_counts()
            print("Actions without price breakdown:")
            for action, count in no_price_actions.items():
                print(f"  {action}: {count}")
        
        # 4. Zero size orders
        zero_size = self.df[self.df['size'] == 0]
        print(f"\nZero Size Orders: {len(zero_size)}")
        if len(zero_size) > 0:
            zero_size_actions = zero_size['action'].value_counts()
            print("Zero size actions breakdown:")
            for action, count in zero_size_actions.items():
                print(f"  {action}: {count}")
    
    def analyze_timing_patterns(self):
        """Analyze timing patterns in the data"""
        print("\n" + "="*80)
        print("⏰ TIMING PATTERN ANALYSIS")
        print("="*80)
        
        self.df['ts_parsed'] = pd.to_datetime(self.df['ts_event'])
        self.df['hour'] = self.df['ts_parsed'].dt.hour
        self.df['minute'] = self.df['ts_parsed'].dt.minute
        
        # Activity by hour
        hourly_activity = self.df['hour'].value_counts().sort_index()
        print("Activity by Hour:")
        for hour, count in hourly_activity.items():
            print(f"  {hour:02d}:00: {count:,} events")
        
        # Time gaps between events
        self.df_sorted = self.df.sort_values('ts_parsed')
        time_diffs = self.df_sorted['ts_parsed'].diff().dt.total_seconds()
        
        print(f"\nTiming Statistics:")
        print(f"  Average gap between events: {time_diffs.mean():.6f} seconds")
        print(f"  Median gap: {time_diffs.median():.6f} seconds")
        print(f"  Max gap: {time_diffs.max():.6f} seconds")
        print(f"  Min gap: {time_diffs.min():.6f} seconds")
    
    def generate_summary_report(self):
        """Generate a comprehensive summary report"""
        print("\n" + "="*80)
        print("📋 SUMMARY REPORT")
        print("="*80)
        
        total_events = len(self.df)
        actions = self.df['action'].value_counts()
        sides = self.df['side'].value_counts()
        
        print("DATASET OVERVIEW:")
        print(f"  • Total Events: {total_events:,}")
        print(f"  • Date Range: {self.df['ts_event'].min()} to {self.df['ts_event'].max()}")
        print(f"  • Symbol: {self.df['symbol'].iloc[0]}")
        
        print("\nACTION BREAKDOWN:")
        for action, count in actions.items():
            pct = (count/total_events)*100
            print(f"  • {action}: {count:,} ({pct:.1f}%)")
        
        print("\nSIDE BREAKDOWN:")
        for side, count in sides.items():
            pct = (count/total_events)*100
            side_name = {'A': 'Ask', 'B': 'Bid', 'N': 'Neutral'}.get(side, side)
            print(f"  • {side} ({side_name}): {count:,} ({pct:.1f}%)")
        
        # Calculate key metrics for reconstruction
        add_orders = actions.get('A', 0)
        cancel_orders = actions.get('C', 0)
        trades = actions.get('T', 0)
        fills = actions.get('F', 0)
        
        print("\nRECONSTRUCTION INSIGHTS:")
        print(f"  • Add Orders (A): {add_orders:,}")
        print(f"  • Cancel Orders (C): {cancel_orders:,}")
        print(f"  • Trades (T): {trades:,}")
        print(f"  • Fills (F): {fills:,}")
        print(f"  • Net Orders (A-C): {add_orders - cancel_orders:,}")
        
        if trades > 0 and fills > 0:
            print(f"  • Potential T->F->C sequences: Up to {min(trades, fills):,}")
    
    def run_full_analysis(self):
        """Run the complete analysis suite"""
        print("🚀 Starting Comprehensive MBO Data Analysis")
        print("="*80)
        
        if self.df is None:
            print("❌ No data loaded. Exiting.")
            return
        
        self.basic_info()
        self.analyze_actions()
        self.analyze_sides()
        self.analyze_prices_and_sizes()
        self.analyze_trade_sequences()
        self.analyze_special_cases()
        self.analyze_timing_patterns()
        self.generate_summary_report()
        
        print("\n" + "="*80)
        print("✅ Analysis Complete!")
        print("="*80)

def main():
    """Main function to run the analysis"""
    csv_file = r"c:\Users\arunk\Cpp-hft-orderbook-engine-\quant_dev_trial\mbo.csv"
    
    try:
        analyzer = MBOAnalyzer(csv_file)
        analyzer.run_full_analysis()
    except Exception as e:
        print(f"❌ Error during analysis: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
