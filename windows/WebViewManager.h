// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "WebView.h"

#include <winrt/Microsoft.ReactNative.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation::Collections;
} // namespace winrt

template <>
struct json_type_traits<winrt::ReactNativeWebView::WebSource> {
  static winrt::ReactNativeWebView::WebSource parseJson(const folly::dynamic &json) {
    winrt::ReactNativeWebView::WebSource source;
    for (auto &item : json.items()) {
      if (item.first == "uri")
        source.uri = item.second.asString();
      if (item.first == "__packager_asset")
        source.packagerAsset = item.second.asBool();
    }
    return source;
  }
};

namespace winrt::ReactNativeWebView {
struct WebSource {
  std::string uri;
  bool packagerAsset;
};
} // namespace winrt::ReactNativeWebView

namespace winrt::ReactNativeWebView::implementation {

class WebViewManager : winrt::implements<
                           WebViewManager,
                           winrt::Microsoft::ReactNative::IViewManager,
                           winrt::Microsoft::ReactNative::IViewManagerWithReactContext,
                           winrt::Microsoft::ReactNative::IViewManagerWithCommands,
                           winrt::Microsoft::ReactNative::IViewManagerWithNativeProperties,
                           winrt::Microsoft::ReactNative::IViewManagerWithExportedEventTypeConstants> {
 public:
  RCT_BEGIN_PROPERTY_MAP(WebViewManager)
  RCT_PROPERTY("source", setSource) RCT_END_PROPERTY_MAP();

  WebViewManager();

  // IViewManager
  winrt::hstring Name() noexcept;
  FrameworkElement CreateView() noexcept;

  // IViewManagerWithReactContext
  winrt::Microsoft::ReactNative::IReactContext ReactContext() noexcept;
  void ReactContext(winrt::Microsoft::ReactNative::IReactContext reactContext) noexcept;

  // IViewManagerWithCommands
  winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, int64_t> Commands() noexcept;

  void DispatchCommand(
      winrt::Windows::UI::Xaml::FrameworkElement const &view,
      int64_t commandId,
      winrt::Microsoft::ReactNative::IJSValueReader const &commandArgsReader) noexcept;

  // IViewManagerWithNativeProperties
  winrt::Windows::Foundation::Collections::
      IMapView<winrt::hstring, winrt::Microsoft::ReactNative::ViewManagerPropertyType>
      NativeProps() noexcept;

  void UpdateProperties(
      winrt::Windows::UI::Xaml::FrameworkElement const &view,
      winrt::Microsoft::ReactNative::IJSValueReader const &propertyMapReader) noexcept;

  // IViewManagerWithExportedEventTypeConstants
  winrt::Microsoft::ReactNative::ConstantProviderDelegate ExportedCustomBubblingEventTypeConstants() noexcept;

  winrt::Microsoft::ReactNative::ConstantProviderDelegate ExportedCustomDirectEventTypeConstants() noexcept;

 private:
  void setSource(XamlView viewToUpdate, const WebSource &sources);
  winrt::Microsoft::ReactNative::IReactContext m_reactContext{nullptr};
};
} // namespace winrt::ReactNativeWebView::implementation
