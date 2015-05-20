// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows Vista uses the Native Wifi (WLAN) API for accessing WiFi cards. See
// http://msdn.microsoft.com/en-us/library/ms705945(VS.85).aspx. Windows XP
// Service Pack 3 (and Windows XP Service Pack 2, if upgraded with a hot fix)
// also support a limited version of the WLAN API. See
// http://msdn.microsoft.com/en-us/library/bb204766.aspx. The WLAN API uses
// wlanapi.h, which is not part of the SDK used by Gears, so is replicated
// locally using data from the MSDN.
//
// Windows XP from Service Pack 2 onwards supports the Wireless Zero
// Configuration (WZC) programming interface. See
// http://msdn.microsoft.com/en-us/library/ms706587(VS.85).aspx.
//
// The MSDN recommends that one use the WLAN API where available, and WZC
// otherwise.
//
// However, it seems that WZC fails for some wireless cards. Also, WLAN seems
// not to work on XP SP3. So we use WLAN on Vista, and use NDIS directly
// otherwise.

//#include "content/browser/geolocation/wifi_data_provider_win.h"
#include "StdAfx.h"
#include <windows.h>
#include <winioctl.h>
#include <wlanapi.h>

//#include "base/utf_string_conversions.h"
//#include "base/win/windows_version.h"
//#include "content/browser/geolocation/wifi_data_provider_common.h"
//#include "content/browser/geolocation/wifi_data_provider_common_win.h"

#include <string>
#include <vector>
#include "assert.h"

#define DCHECK assert

// Taken from ndis.h for WinCE.
#define NDIS_STATUS_INVALID_LENGTH   ((NDIS_STATUS)0xC0010014L)
#define NDIS_STATUS_BUFFER_TOO_SHORT ((NDIS_STATUS)0xC0010016L)

namespace {
// The limits on the size of the buffer used for the OID query.
const int kInitialBufferSize = 2 << 12;  // Good for about 50 APs.
const int kMaximumBufferSize = 2 << 20;  // 2MB

// Length for generic string buffers passed to Win32 APIs.
const int kStringLength = 512;

// The time periods, in milliseconds, between successive polls of the wifi data.
const int kDefaultPollingInterval = 10000;  // 10s
const int kNoChangePollingInterval = 120000;  // 2 mins
const int kTwoNoChangePollingInterval = 600000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

// WlanOpenHandle
typedef DWORD (WINAPI* WlanOpenHandleFunction)(DWORD dwClientVersion,
                                               PVOID pReserved,
                                               PDWORD pdwNegotiatedVersion,
                                               PHANDLE phClientHandle);

// WlanEnumInterfaces
typedef DWORD (WINAPI* WlanEnumInterfacesFunction)(
    HANDLE hClientHandle,
    PVOID pReserved,
    PWLAN_INTERFACE_INFO_LIST* ppInterfaceList);

// WlanGetNetworkBssList
typedef DWORD (WINAPI* WlanGetNetworkBssListFunction)(
    HANDLE hClientHandle,
    const GUID* pInterfaceGuid,
    const  PDOT11_SSID pDot11Ssid,
    DOT11_BSS_TYPE dot11BssType,
    BOOL bSecurityEnabled,
    PVOID pReserved,
    PWLAN_BSS_LIST* ppWlanBssList
);

// WlanFreeMemory
typedef VOID (WINAPI* WlanFreeMemoryFunction)(PVOID pMemory);

// WlanCloseHandle
typedef DWORD (WINAPI* WlanCloseHandleFunction)(HANDLE hClientHandle,
                                                PVOID pReserved);

class AccessPoint {
public:
  std::string mac_address;
  int radio_signal_strength;
  std::string ssid;

};

#define string16 std::wstring

class WindowsNdisApi {
 public:
  virtual ~WindowsNdisApi();
  static WindowsNdisApi* Create();

  // WlanApiInterface
  virtual bool GetAccessPointData(std::vector<AccessPoint>& data);

 private:
  static bool GetInterfacesNDIS(
      std::vector<string16>* interface_service_names_out);

  // Swaps in content of the vector passed
  explicit WindowsNdisApi(std::vector<string16>* interface_service_names);

  bool GetInterfaceDataNDIS(HANDLE adapter_handle,
                            std::vector<AccessPoint>& data);
  // NDIS variables.
  std::vector<string16> interface_service_names_;

  // Remembers scan result buffer size across calls.
  /////int oid_buffer_size_;

  std::vector<char> _buffer;
};

// Extracts data for an access point and converts to Gears format.
bool GetNetworkData(const WLAN_BSS_ENTRY& bss_entry,
                    std::vector<AccessPoint>& access_point_data);
bool UndefineDosDevice(const string16& device_name);
bool DefineDosDeviceIfNotExists(const string16& device_name);
HANDLE GetFileHandle(const string16& device_name);
// Makes the OID query and returns a Win32 error code.
int PerformQuery(HANDLE adapter_handle, std::vector<char>& buffer, DWORD* bytes_out);
bool ResizeBuffer(size_t requested_size, std::vector<char>& buffer);
// Gets the system directory and appends a trailing slash if not already
// present.
bool GetSystemDirectory(string16* path);
}  // namespace

// WindowsNdisApi
WindowsNdisApi::WindowsNdisApi(
    std::vector<string16>* interface_service_names)
    : _buffer(kInitialBufferSize) {
  assert(!interface_service_names->empty());
  interface_service_names_.swap(*interface_service_names);
}

WindowsNdisApi::~WindowsNdisApi() {
}

WindowsNdisApi* WindowsNdisApi::Create() {
  std::vector<string16> interface_service_names;
  if (GetInterfacesNDIS(&interface_service_names)) {
    return new WindowsNdisApi(&interface_service_names);
  }
  return NULL;
}

bool WindowsNdisApi::GetAccessPointData(std::vector<AccessPoint>& data) {
  //DCHECK(data);
  int interfaces_failed = 0;
  int interfaces_succeeded = 0;

  for (int i = 0; i < static_cast<int>(interface_service_names_.size()); ++i) {
    // First, check that we have a DOS device for this adapter.
    if (!DefineDosDeviceIfNotExists(interface_service_names_[i])) {
      continue;
    }

    // Get the handle to the device. This will fail if the named device is not
    // valid.
    HANDLE adapter_handle = GetFileHandle(interface_service_names_[i]);
    if (adapter_handle == INVALID_HANDLE_VALUE) {
      continue;
    }

    // Get the data.
    if (GetInterfaceDataNDIS(adapter_handle, data)) {
      ++interfaces_succeeded;
    } else {
      ++interfaces_failed;
    }

    // Clean up.
    CloseHandle(adapter_handle);
    UndefineDosDevice(interface_service_names_[i]);
  }

  // Return true if at least one interface succeeded, or at the very least none
  // failed.
  return interfaces_succeeded > 0 || interfaces_failed == 0;
}

bool WindowsNdisApi::GetInterfacesNDIS(std::vector<string16>* interface_service_names_out) {
  HKEY network_cards_key = NULL;
  if (RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards",
      0,
      KEY_READ,
      &network_cards_key) != ERROR_SUCCESS) {
    return false;
  }
  DCHECK(network_cards_key);

  for (int i = 0; ; ++i) {
    TCHAR name[kStringLength];
    DWORD name_size = kStringLength;
    FILETIME time;
    if (RegEnumKeyEx(network_cards_key,
                     i,
                     name,
                     &name_size,
                     NULL,
                     NULL,
                     NULL,
                     &time) != ERROR_SUCCESS) {
      break;
    }
    HKEY hardware_key = NULL;
    if (RegOpenKeyEx(network_cards_key, name, 0, KEY_READ, &hardware_key) !=
        ERROR_SUCCESS) {
      break;
    }
    DCHECK(hardware_key);

    TCHAR service_name[kStringLength];
    DWORD service_name_size = kStringLength;
    DWORD type = 0;
    if (RegQueryValueEx(hardware_key,
                        L"ServiceName",
                        NULL,
                        &type,
                        reinterpret_cast<LPBYTE>(service_name),
                        &service_name_size) == ERROR_SUCCESS) {
      interface_service_names_out->push_back(service_name);
    }
    RegCloseKey(hardware_key);
  }

  RegCloseKey(network_cards_key);
  return true;
}
#define uint8 unsigned char

//std::wstring AUTF8ToUTF16(const std::string &data)
//{
//    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(data);
//}
#define MAC_AS_NUM_LEN 6
static std::string MacAddressAsString(const unsigned char macAsNumber[MAC_AS_NUM_LEN])
{
  const int charlen = MAC_AS_NUM_LEN * 2;
  std::string result = std::string(charlen, ' ');
  const char hexmap[] = { '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };  
  for (int i = 0; i < MAC_AS_NUM_LEN; ++i) {
    result[2 * i] = hexmap[(macAsNumber[i] & 0xF0) >> 4];
    result[2 * i + 1] = hexmap[macAsNumber[i] & 0x0F];
  }
  return result;
}

bool ConvertToAccessPointData(const NDIS_WLAN_BSSID& data, AccessPoint& access_point_data) 
{
	access_point_data.mac_address = MacAddressAsString(data.MacAddress);
	access_point_data.radio_signal_strength = data.Rssi;
	// Note that _NDIS_802_11_SSID::Ssid::Ssid is not null-terminated.
  const unsigned char* ssid = data.Ssid.Ssid;
  size_t len = data.Ssid.SsidLength;
  access_point_data.ssid = std::string(reinterpret_cast<const char*>(ssid), len);

	return true;
}

int GetDataFromBssIdList(const NDIS_802_11_BSSID_LIST& bss_id_list,
                         int list_size,
                         std::vector<AccessPoint>& data) 
{
  // Walk through the BSS IDs.
  int found = 0;
  const uint8* iterator = reinterpret_cast<const uint8*>(&bss_id_list.Bssid[0]);
  const uint8* end_of_buffer =
      reinterpret_cast<const uint8*>(&bss_id_list) + list_size;
  for (int i = 0; i < static_cast<int>(bss_id_list.NumberOfItems); ++i) {
    const NDIS_WLAN_BSSID *bss_id =
        reinterpret_cast<const NDIS_WLAN_BSSID*>(iterator);
    // Check that the length of this BSS ID is reasonable.
    if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) ||
        iterator + bss_id->Length > end_of_buffer) {
      break;
    }
    AccessPoint access_point_data;
    if (ConvertToAccessPointData(*bss_id, access_point_data)) {
      data.push_back(access_point_data);
      ++found;
    }
    // Move to the next BSS ID.
    iterator += bss_id->Length;
  }
  return found;
}



bool WindowsNdisApi::GetInterfaceDataNDIS(HANDLE adapter_handle,
                                          std::vector<AccessPoint>& data) {
  //DCHECK(data);
  DWORD bytes_out;
  int result;

  while (true) {
    bytes_out = 0;
    result = PerformQuery(adapter_handle, _buffer, &bytes_out);
    if (result == ERROR_GEN_FAILURE ||  // Returned by some Intel cards.
        result == ERROR_INSUFFICIENT_BUFFER ||
        result == ERROR_MORE_DATA ||
        result == NDIS_STATUS_INVALID_LENGTH ||
        result == NDIS_STATUS_BUFFER_TOO_SHORT) {
      // The buffer we supplied is too small, so increase it. bytes_out should
      // provide the required buffer size, but this is not always the case.
      size_t newSize;
      if (bytes_out > static_cast<DWORD>(_buffer.size())) {
        newSize = bytes_out;
      } else {
        newSize = _buffer.size() * 2;
      }
      if (!ResizeBuffer(newSize, _buffer)) {
        return false;
      }
    } else {
      // The buffer is not too small.
      break;
    }
  }
  //DCHECK(buffer.get());

  if (result == ERROR_SUCCESS) {
    NDIS_802_11_BSSID_LIST* bssid_list = 
        reinterpret_cast<NDIS_802_11_BSSID_LIST*>(&_buffer[0]);
    GetDataFromBssIdList(*bssid_list, _buffer.size(), data);
  }

  return true;
}

namespace {

bool GetNetworkData(const WLAN_BSS_ENTRY& bss_entry, AccessPoint& access_point_data) {
  // Currently we get only MAC address, signal strength and SSID.
  //DCHECK(access_point_data);
  access_point_data.mac_address = MacAddressAsString(bss_entry.dot11Bssid);
  access_point_data.radio_signal_strength = bss_entry.lRssi;
  // bss_entry.dot11Ssid.ucSSID is not null-terminated.
  access_point_data.ssid = std::string(reinterpret_cast<const char*>(bss_entry.dot11Ssid.ucSSID),
                                       static_cast<ULONG>(bss_entry.dot11Ssid.uSSIDLength));

 // UTF8ToUTF16(reinterpret_cast<const char*>(bss_entry.dot11Ssid.ucSSID),
   //           static_cast<ULONG>(bss_entry.dot11Ssid.uSSIDLength),
     //         &access_point_data->ssid);
  // TODO(steveblock): Is it possible to get the following?
  // access_point_data->signal_to_noise
  // access_point_data->age
  // access_point_data->channel
  return true;
}

bool UndefineDosDevice(const string16& device_name) {
  // We remove only the mapping we use, that is \Device\<device_name>.
  string16 target_path = L"\\Device\\" + device_name;
  return DefineDosDevice(
      DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
      device_name.c_str(),
      target_path.c_str()) == TRUE;
}

bool DefineDosDeviceIfNotExists(const string16& device_name) {
  // We create a DOS device name for the device at \Device\<device_name>.
  string16 target_path = L"\\Device\\" + device_name;

  TCHAR target[kStringLength];
  if (QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0) {
    // Device already exists.
    return true;
  }

  if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return false;
  }

  if (!DefineDosDevice(DDD_RAW_TARGET_PATH,
                       device_name.c_str(),
                       target_path.c_str())) {
    return false;
  }

  // Check that the device is really there.
  return QueryDosDevice(device_name.c_str(), target, kStringLength) > 0 &&
      target_path.compare(target) == 0;
}

HANDLE GetFileHandle(const string16& device_name) {
  // We access a device with DOS path \Device\<device_name> at
  // \\.\<device_name>.
  string16 formatted_device_name = L"\\\\.\\" + device_name;

  return CreateFile(formatted_device_name.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,  // share mode
                    0,  // security attributes
                    OPEN_EXISTING,
                    0,  // flags and attributes
                    INVALID_HANDLE_VALUE);
}

int PerformQuery(HANDLE adapter_handle,
                 std::vector<char>& buffer,
                 DWORD* bytes_out) {
  DWORD oid = OID_802_11_BSSID_LIST;
  if (!DeviceIoControl(adapter_handle,
                       IOCTL_NDIS_QUERY_GLOBAL_STATS,
                       &oid,
                       sizeof(oid),
                       &buffer[0],
                       buffer.size(),
                       bytes_out,
                       NULL)) {
    return GetLastError();
  }
  return ERROR_SUCCESS;
}

bool ResizeBuffer(size_t requested_size, std::vector<char>& buffer) {
  //DCHECK_GT(requested_size, 0);
  //DCHECK(buffer);
  if (requested_size > kMaximumBufferSize) {
    buffer.resize(kInitialBufferSize);
    return false;
  }

 // buffer->reset(reinterpret_cast<BYTE*>(realloc(buffer->release(), requested_size)));
  buffer.resize(requested_size);
  return true;
}

//bool GetSystemDirectory(string16* path) {
//  DCHECK(path);
//  // Return value includes terminating NULL.
//  int buffer_size = ::GetSystemDirectory(NULL, 0);
//  if (buffer_size == 0) {
//    return false;
//  }
//  scoped_array<char16> buffer(new char16[buffer_size]);
//
//  // Return value excludes terminating NULL.
//  int characters_written = ::GetSystemDirectory(buffer.get(), buffer_size);
//  if (characters_written == 0) {
//    return false;
//  }
//  DCHECK_EQ(buffer_size - 1, characters_written);
//
//  path->assign(buffer.get(), characters_written);
//
//  if (*path->rbegin() != L'\\') {
//    path->append(L"\\");
//  }
//  DCHECK_EQ(L'\\', *path->rbegin());
//  return true;
//}
}  // namespace