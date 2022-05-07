/* 
 *
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
	To do:
	- implement remove()
	- fix set() to replace existing values
 */
/*************************************************************************************************************************************
 * Ported from https://android.googlesource.com/platform/frameworks/base/+/26d4452/core/java/android/net/nsd/DnsSdTxtRecord.java
 *************************************************************************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <memory.h>
#include <sstream>

namespace p2p
{
	class DnsSdTxtRecord
	{

		/* 
	DNS-SD specifies that a TXT record corresponding to an SRV record consist of
	a packed array of bytes, each preceded by a length byte. Each string
	is an attribute-value pair. 
	
	The TXTRecord object stores the entire TXT data as a single byte array, traversing it
	as need be to implement its various methods.
	*/
	public:
		using byte = uint8_t;

	private:
		static std::vector<byte> getBytes(const std::string &v) 
		{
			std::vector<byte> result;
			result.resize(v.size());
			for (size_t i = 0; i < v.size(); ++i)
			{
				result[i] = (byte)v.at(i);
			}
			return result;
		}
	protected:
		static constexpr byte kAttrSep = '=';
	private:
		std::vector<byte> fBytes;

	public:
		/** Constructs a new, empty TXT record. */
		DnsSdTxtRecord()
		{
		}
		/** Constructs a new TXT record from a byte array in the standard format. */
		DnsSdTxtRecord(const std::vector<byte> &initBytes)
			: fBytes(initBytes)
		{
		}
		/** Set a key/value pair in the TXT record. Setting an existing key will replace its value.<P>
				@param	key
							The key name. Must be ASCII, with no '=' characters.
				<P>
				@param	value
							Value to be encoded into bytes using the default platform character set.
			*/
		void set(const std::string& key, const std::string& value)
		{
			std::vector<byte> valBytes = getBytes(value);
			this->set(key, valBytes);
		}
		/** Set a key/value pair in the TXT record. Setting an existing key will replace its value.<P>
				@param	key
							The key name. Must be ASCII, with no '=' characters.
				<P>
				@param	value
					Binary representation of the value.
			*/
		void set(const std::string& key, const std::vector<byte> &value)
		{
			std::vector<byte> keyBytes;
			size_t valLen = value.size();

			keyBytes = getBytes(key);

			for (size_t i = 0; i < keyBytes.size(); i++)
				if (keyBytes[i] == '=')
					throw std::invalid_argument("Bad txt record.");
			if (keyBytes.size() + valLen >= 255)
				throw std::invalid_argument("Bad txt record.");
			ssize_t prevLoc = this->remove(key);
			if (prevLoc == -1)
				prevLoc = this->size();
			this->insert(keyBytes, value, prevLoc);
		}
		void insert(const std::vector<byte> &keyBytes, const std::vector<byte> &value, size_t index)
		// Insert a key-value pair at index
		{
			std::vector<byte> oldBytes = fBytes;
			size_t valLen = value.size();
			size_t insertion = 0;
			size_t newLen, avLen;

			// locate the insertion point
			for (size_t i = 0; i < index && insertion < fBytes.size(); i++)
				insertion += (0xFF & (fBytes[insertion] + 1));

			avLen = keyBytes.size() + valLen + 1;
			newLen = avLen + oldBytes.size() + 1;
			fBytes.resize(0);
			fBytes.resize(newLen);
			memcpy(&fBytes[0], &oldBytes[0], insertion);
			int secondHalfLen = oldBytes.size() - insertion;
			memcpy(&fBytes[newLen - secondHalfLen], &oldBytes[insertion], secondHalfLen);
			fBytes[insertion] = (byte)avLen;
			memcpy(&fBytes[insertion+1],&keyBytes[0], keyBytes.size());
			fBytes[insertion + 1 + keyBytes.size()] = kAttrSep;
			memcpy(&fBytes[insertion + keyBytes.size() + 2],&value[0], valLen);
		}
		/** Remove a key/value pair from the TXT record. Returns index it was at, or -1 if not found. */
	public:
		ssize_t remove(const std::string& key)
		{
			size_t avStart = 0;
			for (size_t i = 0; avStart < fBytes.size(); i++)
			{
				size_t avLen = fBytes[avStart];
				if (key.size() <= avLen &&
					(key.size() == avLen || fBytes[avStart + key.size() + 1] == kAttrSep))
				{
					std::string s { (const char*) (&fBytes[avStart + 1]), key.size() };
					if (caseInsensitiveCompare(key,s))
					{
						std::vector<byte> oldBytes = fBytes;
						fBytes.resize(0);
						fBytes.resize(oldBytes.size() - avLen - 1);
						memcpy(&fBytes[0],&oldBytes[0],avStart);
						memcpy(&fBytes[avStart],&oldBytes[avStart+avLen+1],oldBytes.size() - avStart - avLen - 1);
						return i;
					}
				}
				avStart += (0xFF & (avLen + 1));
			}
			return -1;
		}
		/**	Return the number of keys in the TXT record. */
	public:
		int size() const
		{
			size_t i, avStart;
			for (i = 0, avStart = 0; avStart < fBytes.size(); i++)
				avStart += (0xFF & (fBytes[avStart] + 1));
			return i;
		}
		/** Return true if key is present in the TXT record, false if not. */
	public:
		bool contains(const std::string& key) const
		{
			std::string s = "";
			for (size_t i = 0; "" != (s = this->getKey(i)); i++)
				if (caseInsensitiveCompare(key,s))
					return true;
			return false;
		}
		/**	Return a key in the TXT record by zero-based index. Returns null if index exceeds the total number of keys. */
	public:
		std::string getKey(size_t index) const
		{
			size_t avStart = 0;
			for (size_t i = 0; i < index && avStart < fBytes.size(); i++)
				avStart += fBytes[avStart] + 1;
			if (avStart < fBytes.size())
			{
				size_t avLen = fBytes[avStart];
				size_t aLen = 0;

				for (aLen = 0; aLen < avLen; aLen++)
					if (fBytes[avStart + aLen + 1] == kAttrSep)
						break;
				return std::string((const char*)(&fBytes[avStart + 1]), aLen);
			}
			return "";
		}
		/**	
		Look up a key in the TXT record by zero-based index and return its value. <P>
		Returns null if index exceeds the total number of keys. 
		Returns null if the key is present with no value.
	*/
	public:
		std::vector<byte> getValue(size_t index) const
		{
			size_t avStart = 0;
			std::vector<byte> value;
			for (size_t i = 0; i < index && avStart < fBytes.size(); i++)
				avStart += fBytes[avStart] + 1;
			if (avStart < fBytes.size())
			{
				size_t avLen = fBytes[avStart];
				size_t aLen = 0;

				for (aLen = 0; aLen < avLen; aLen++)
				{
					if (fBytes[avStart + aLen + 1] == kAttrSep)
					{
						value.resize(avLen - aLen - 1);
						memcpy(&value[0],&fBytes[avStart + aLen + 2],avLen - aLen - 1);
						break;
					}
				}
			}
			return value;
		}
		/** Converts the result of getValue() to a string in the platform default character set. (not really) */
	public:
		std::string getValueAsString(int index) const
		{
			std::vector<byte> value = this->getValue(index);
			return std::string((const char*)&value[0],value.size());
		}
		/**	Get the value associated with a key. Will be null if the key is not defined.
		Array will have length 0 if the key is defined with an = but no value.<P> 
		@param	forKey
					The left-hand side of the key-value pair.
		<P>
		@return		The binary representation of the value.
	*/
	public:
		std::vector<byte> getValue(const std::string& forKey) const
		{
			std::string s;
			int i;
			for (i = 0; "" != (s = this->getKey(i)); i++)
				if (caseInsensitiveCompare(forKey,s))
					return this->getValue(i);
			return std::vector<byte>();
		}
		/**	Converts the result of getValue() to a string in the platform default character set.<P> 
		@param	forKey
					The left-hand side of the key-value pair.
		<P>
		@return		The value represented in the default platform character set (without translating '.'s, so not so useful)
	*/
	public:
		std::string getValueAsString(const std::string& forKey)
		{
			std::vector<byte> val = this->getValue(forKey);
			return std::string((const char*)&val[0],val.size());
		}
		/** Return the contents of the TXT record as raw bytes. */
	public:
		const std::vector<byte> &getRawBytes() const { return fBytes;}
		/** Return a string representation of the object. */
	public:
		std::string toString()
		{
			std::stringstream result;
			std::string a;

			for (size_t i = 0; "" != (a = this->getKey(i)); i++)
			{
				if (i != 0) result << ", ";
				std::string val = this->getValueAsString(i);

				result << i << "={" << a << '=' << val << "}";
			}
			std::string t = result.str();
			if (t.length() == 0) return "<empty>";
			return t;
		}
	};
}