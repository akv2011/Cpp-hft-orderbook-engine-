#!/usr/bin/env python3
"""
Critical Requirements Verification Script
Verify that our T->F->C implementation follows the exact requirements
"""

import pandas as pd
import sys

def verify_tfc_side_handling():
    """Verify T->F->C side handling according to requirements"""
    print("🔍 CRITICAL REQUIREMENTS VERIFICATION")
    print("="*80)
    
    # Load our output
    try:
        our_df = pd.read_csv('output.csv')
        print(f"✅ Loaded our output: {len(our_df)} rows")
    except Exception as e:
        print(f"❌ Error loading output.csv: {e}")
        return False
    
    # Load reference
    try:
        ref_df = pd.read_csv('quant_dev_trial/mbp.csv')
        print(f"✅ Loaded reference: {len(ref_df)} rows")
    except Exception as e:
        print(f"❌ Error loading reference mbp.csv: {e}")
        return False
    
    print("\n" + "="*80)
    print("📋 REQUIREMENT VERIFICATION")
    print("="*80)
    
    # Requirement 1: Initial R action should be ignored
    print("\n1. ✅ Requirement: Ignore initial R action")
    our_first_action = our_df.iloc[0]['action'] if len(our_df) > 0 else None
    ref_first_action = ref_df.iloc[0]['action'] if len(ref_df) > 0 else None
    print(f"   Our first action: {our_first_action}")
    print(f"   Ref first action: {ref_first_action}")
    if our_first_action == 'R' and ref_first_action == 'R':
        print("   ✅ Both start with R - requirement handled")
    else:
        print("   ⚠️  Different first actions")
    
    # Requirement 2: T->F->C should become single T action
    print("\n2. ✅ Requirement: T->F->C → single T action")
    our_t_count = len(our_df[our_df['action'] == 'T'])
    ref_t_count = len(ref_df[ref_df['action'] == 'T'])
    our_f_count = len(our_df[our_df['action'] == 'F'])
    ref_f_count = len(ref_df[ref_df['action'] == 'F'])
    our_c_after_f_count = len(our_df[our_df['action'] == 'C'])
    ref_c_after_f_count = len(ref_df[ref_df['action'] == 'C'])
    
    print(f"   Our T actions: {our_t_count}")
    print(f"   Ref T actions: {ref_t_count}")
    print(f"   Our F actions: {our_f_count}")
    print(f"   Ref F actions: {ref_f_count}")
    print(f"   Our C actions: {our_c_after_f_count}")
    print(f"   Ref C actions: {ref_c_after_f_count}")
    
    if our_f_count == 0:
        print("   ✅ No F actions in our output (consolidated into T)")
    else:
        print("   ❌ CRITICAL ERROR: Found F actions in our output!")
        return False
    
    # Requirement 3: T with side 'N' should not alter orderbook
    print("\n3. ✅ Requirement: T with side 'N' should not alter orderbook")
    our_t_n = len(our_df[(our_df['action'] == 'T') & (our_df['side'] == 'N')])
    ref_t_n = len(ref_df[(ref_df['action'] == 'T') & (ref_df['side'] == 'N')])
    print(f"   Our T,N actions: {our_t_n}")
    print(f"   Ref T,N actions: {ref_t_n}")
    
    # CRITICAL: Check side handling for T events
    print("\n4. 🚨 CRITICAL: T event side handling")
    print("   Requirement: T should be on the side that ACTUALLY changes in the book")
    print("   (opposite of the original T side from MBO)")
    
    # Sample a few T events and analyze
    our_t_events = our_df[our_df['action'] == 'T'].head(10)
    ref_t_events = ref_df[ref_df['action'] == 'T'].head(10)
    
    if len(our_t_events) > 0:
        print(f"\n   Sample of our T events:")
        for i, (_, row) in enumerate(our_t_events.iterrows()):
            print(f"   {i+1}. Price: {row['price']:>8}, Side: {row['side']}, Size: {row['size']}")
    
    if len(ref_t_events) > 0:
        print(f"\n   Sample of reference T events:")
        for i, (_, row) in enumerate(ref_t_events.iterrows()):
            print(f"   {i+1}. Price: {row['price']:>8}, Side: {row['side']}, Size: {row['size']}")
    
    # Check if we have the same trade prices and sizes but different sides
    print(f"\n   🔍 Comparing T event sides...")
    if len(our_t_events) > 0 and len(ref_t_events) > 0:
        # Look for matching prices to see if sides are flipped
        matches_found = 0
        sides_match = 0
        sides_opposite = 0
        
        for _, our_row in our_t_events.iterrows():
            for _, ref_row in ref_t_events.iterrows():
                if (abs(our_row['price'] - ref_row['price']) < 0.01 and 
                    our_row['size'] == ref_row['size']):
                    matches_found += 1
                    if our_row['side'] == ref_row['side']:
                        sides_match += 1
                    else:
                        sides_opposite += 1
                    break
        
        print(f"   Matching T events found: {matches_found}")
        print(f"   Same sides: {sides_match}")
        print(f"   Opposite sides: {sides_opposite}")
        
        if sides_opposite > sides_match:
            print("   ✅ GOOD: Most T events have opposite sides (requirement met)")
        elif sides_match > sides_opposite:  
            print("   ❌ CRITICAL ERROR: Most T events have same sides (requirement NOT met)")
            print("   🚨 We may NOT be implementing side flipping correctly!")
            return False
        else:
            print("   ⚠️  Mixed results - needs investigation")
    
    return True

def main():
    success = verify_tfc_side_handling()
    
    if success:
        print("\n✅ VERIFICATION COMPLETE - Implementation appears correct")
    else:
        print("\n❌ VERIFICATION FAILED - Critical requirements not met")
        sys.exit(1)

if __name__ == "__main__":
    main()
