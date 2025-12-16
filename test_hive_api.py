#!/usr/bin/env python3
"""
Test script to verify Hive API key is working correctly.
"""
import os
import sys
import requests
import json

def test_hive_api():
    # Get API key from environment
    api_key = os.environ.get('MODAI_HIVE_API_KEY')
    
    if not api_key:
        print("❌ Error: MODAI_HIVE_API_KEY environment variable not set")
        print("\nPlease set it with:")
        print('  export MODAI_HIVE_API_KEY="your_key_here"')
        return False
    
    print(f"🔑 Using API key: {api_key[:10]}...{api_key[-5:]}")
    print("\n📡 Testing Hive API...")
    
    # Test with a simple text
    test_text = "This is a test message to verify the API key works correctly."
    
    url = "https://api.thehive.ai/api/v3/hive/text-moderation"
    
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    
    payload = {
        "input": [
            {
                "text": test_text
            }
        ]
    }
    
    try:
        print(f"🚀 Sending request to: {url}")
        response = requests.post(url, headers=headers, json=payload, timeout=10)
        
        print(f"📊 Response Status: {response.status_code}")
        
        if response.status_code == 200:
            print("✅ SUCCESS! API key is working correctly.\n")
            
            try:
                data = response.json()
                print("📋 Response data:")
                print(json.dumps(data, indent=2))
                
                # Check for labels
                if 'status' in data:
                    for status_item in data['status']:
                        print(f"\n✓ Task: {status_item.get('response', {}).get('output', [{}])[0].get('time', 'N/A')}")
                
            except json.JSONDecodeError:
                print("Response (raw):")
                print(response.text[:500])
            
            return True
            
        elif response.status_code == 401:
            print("❌ FAILED: Authentication error - API key is invalid")
            print(f"Response: {response.text}")
            return False
            
        elif response.status_code == 403:
            print("❌ FAILED: Forbidden - API key may not have required permissions")
            print(f"Response: {response.text}")
            return False
            
        else:
            print(f"⚠️  Unexpected status code: {response.status_code}")
            print(f"Response: {response.text}")
            return False
            
    except requests.exceptions.Timeout:
        print("❌ FAILED: Request timed out")
        return False
        
    except requests.exceptions.ConnectionError:
        print("❌ FAILED: Could not connect to Hive API")
        return False
        
    except Exception as e:
        print(f"❌ FAILED: {type(e).__name__}: {e}")
        return False

if __name__ == "__main__":
    print("=" * 60)
    print("  Hive API Key Test")
    print("=" * 60 + "\n")
    
    success = test_hive_api()
    
    print("\n" + "=" * 60)
    if success:
        print("✅ Test completed successfully!")
    else:
        print("❌ Test failed - please check your API key")
    print("=" * 60)
    
    sys.exit(0 if success else 1)
