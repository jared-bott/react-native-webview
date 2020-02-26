// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "pch.h"

#include "JSValueReader.h"
#include "JSValueXaml.h"
#include "NativeModules.h"
#include "WebViewManager.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>

#include <condition_variable>
#include <mutex>
#include <type_traits>

using namespace folly;

namespace winrt {
using namespace Microsoft::ReactNative;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml::Controls;
} // namespace winrt

namespace winrt::ReactNativeWebView::implementation {

WebViewManager::WebViewManager() {
  SetupPropertyHandlersInternal();
}

winrt::hstring WebViewManager::Name() noexcept {
  return L"RNCWebView";
}

FrameworkElement WebViewManager::CreateView() noexcept {
  auto webView = WebView(m_reactContext);
  return webView;
}

winrt::Microsoft::ReactNative::IReactContext WebViewManager::ReactContext() noexcept {
  return m_reactContext;
}

void WebViewManager::ReactContext(winrt::Microsoft::ReactNative::IReactContext reactContext) noexcept {
  m_reactContext = reactContext;
}

winrt::Windows::Foundation::Collections::
    IMapView<winrt::hstring, winrt::Microsoft::ReactNative::ViewManagerPropertyType>
    WebViewManager::NativeProps() noexcept {
  auto nativeProps = winrt::single_threaded_map<hstring, ViewManagerPropertyType>();
  nativeProps.Insert(L"source", ViewManagerPropertyType::Map);
  nativeProps.Insert(L"injectedJavaScriptBeforeContentLoaded", ViewManagerPropertyType::String);

  return nativeProps.GetView();
}

void WebViewManager::UpdateProperties(
    winrt::Windows::UI::Xaml::FrameworkElement const &view,
    winrt::Microsoft::ReactNative::IJSValueReader const &propertyMapReader) noexcept {
  if (auto webView = view.try_as<winrt::ReactNativeWebView::WebView>()) {
    const JSValueObject &propertyMap = JSValue::ReadObjectFrom(propertyMapReader);
    bool sourceChanged = false;

    for (auto const &pair : propertyMap) {
      auto const &propertyName = pair.first;
      auto const &propertyValue = pair.second;
      if (!propertyValue.IsNull()) {
        if (propertyName == "source") {
          auto const &srcMap = propertyValue.Object();
          auto uri = to_hstring(srcMap.at("uri").String());
          if (srcMap.at("packagerAsset") && uri.find("assets") == 0) {
            uri.replace(0, 6, "ms-appx://");
          }
          webView.Set_UriString();
          sourceChanged = true;
        } else if (propertyName == "injectedJavaScriptBeforeContentLoaded") {
          webView.Set_StartingJavaScript(propertyValue.String());
        }
      }
    }

    if (sourceChanged) {
      webView.Load();
    }
  }
}

void WebViewManager::setSource(XamlView viewToUpdate, const WebSource &source) {
  auto instance = m_wkReactInstance.lock();
  if (instance == nullptr) {
    return;
  }
  auto view = viewToUpdate.as<winrt::WebView>();

  // non-uri sources not yet supported
  if (source.uri.length() == 0) {
    return;
  }

  auto uriString = source.uri;
  if (source.packagerAsset && uriString.find("assets") == 0) {
    uriString.replace(0, 6, "ms-appx://");
  }

  auto uri = winrt::Uri(winrt::hstring(asWStr(uriString).c_str()));
  view.Navigate(uri);
}

winrt::Microsoft::ReactNative::ConstantProviderDelegate WebViewManager::ExportedViewConstants() noexcept {
  return [](winrt::Microsoft::ReactNative::IJSValueWriter const &constantWriter) {};
}

ConstantProviderDelegate WebViewManager::ExportedCustomDirectEventTypeConstants() noexcept {
  return [](winrt::Microsoft::ReactNative::IJSValueWriter const &constantWriter) {
    WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnLoadingStart);
    WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnLoadingFinish);
    WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnMessage);
    WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnLoadingError);
    WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnHttpError);
    // WriteCustomDirectEventTypeConstant(constantWriter, WebViewEvents::OnShouldStartLoadWithRequest);
  };
}

winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, int64_t> WebViewManager::Commands() noexcept {
  auto commands = winrt::single_threaded_map<hstring, int64_t>();
  commands.Insert(L"injectJavaScript", static_cast<int32_t>(WebViewCommands::InjectJavaScript));
  commands.Insert(L"goBack", static_cast<int32_t>(WebViewCommands::GoBack));
  commands.Insert(L"goForward", static_cast<int32_t>(WebViewCommands::GoForward));
  commands.Insert(L"reload", static_cast<int32_t>(WebViewCommands::Reload));
  commands.Insert(L"stopLoading", static_cast<int32_t>(WebViewCommands::StopLoading));
  return commands.GetView();
}

void WebViewManager::DispatchCommand(
    winrt::Windows::UI::Xaml::FrameworkElement const &view,
    int64_t commandId,
    winrt::Microsoft::ReactNative::IJSValueReader const &commandArgsReader) noexcept {
  if (auto control = view.try_as<WebView>()) {
    switch (commandId) {
      case static_cast<int64_t>(WebViewCommands::InjectJavaScript):
        control.InjectJavaScript(winrt::to_string(commandArgsReader.GetString()));
        break;
      case static_cast<int64_t>(WebViewCommands::GoBack):
        control.GoBack();
        break;
      case static_cast<int64_t>(WebViewCommands::GoForward):
        controlGoForward();
        break;
      case static_cast<int64_t>(WebViewCommands::Reload):
        control.Refresh();
        break;
      case static_cast<int64_t>(WebViewCommands::StopLoading):
        control.Stop();
        break;
    }
  }
}
} // namespace winrt::ReactNativeWebView::implementation
