// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "WebView.g.h"
#include <functional>

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation::Collections;
} // namespace winrt

namespace winrt::ReactNativeWebView::implementation {

struct WebViewEvents {
  static constexpr auto OnLoadingStart = "onLoadingStart";
  static constexpr auto OnLoadingFinish = "onLoadingFinish";
  static constexpr auto OnMessage = "onMessage";
  static constexpr auto OnLoadingError = "onLoadingError";
  static constexpr auto OnHttpError = "onHttpError";
  static constexpr auto OnShouldStartLoadWithRequest = "onShouldStartLoadWithRequest";
};

enum class WebViewCommands : int32_t { InjectJavaScript = 0, GoBack = 1, GoForward = 2, Reload = 3, StopLoading = 4 };

class WebView : public WebViewT<WebView> {
 public:
  WebView(Microsoft::ReactNative::IReactContext const &reactContext);
  ~WebView();

  void Load();
  void Set_UriString(hstring const &uri);
  void Set_StartingJavaScript(hstring const &script);
  void InjectJavaScript(hstring const &script);
  void GoBack();
  void GoForward();
  void Reload();
  void StopLoading();

 private:
  winrt::IAsyncAction injectJavaScript(winrt::hstring javaScript);
  winrt::IAsyncAction injectMessageJavaScript();
  void createWebViewArgs(winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter);
  void runOnQueue(std::function<void()> &&func);

  winrt::event_token m_onMessageToken;
  winrt::event_token m_onLoadedToken;
  winrt::event_token m_onLoadStartToken;
  winrt::event_token m_onErrorToken;
  winrt::hstring m_injectedJavascript;
  winrt::hstring m_uri;
  Windows::UI::Core::CoreDispatcher m_uiDispatcher = nullptr;
  Microsoft::ReactNative::IReactContext m_reactContext{nullptr};
  winrt::WebView m_webView;
};
} // namespace winrt::ReactNativeWebView::implementation
