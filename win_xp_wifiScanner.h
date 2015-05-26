#pragma once

#include <vector>
#include "nsCOMArray.h"

class nsWifiAccessPoint;

class WindowsNdisApi 
{
public:
  virtual ~WindowsNdisApi();
  static WindowsNdisApi* Create();
  virtual bool GetAccessPointData(nsCOMArray<nsWifiAccessPoint>& outData);

private:
  static bool GetInterfacesNDIS(std::vector<std::string>& interface_service_names_out);
  // Swaps in content of the vector passed
  explicit WindowsNdisApi(std::vector<std::string>* interface_service_names);
  bool GetInterfaceDataNDIS(HANDLE adapter_handle, nsCOMArray<nsWifiAccessPoint>& outData);
  // NDIS variables.
  std::vector<std::string> interface_service_names_;
  std::vector<char> _buffer;
};
