// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <Views/FrameworkElementViewManager.h>
#include <Views/ShadowNodeBase.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <string_view>
#include "Utils/PropertyHandlerUtils.h"

namespace winrt::Windows::UI::Xaml::Media {
enum class Stretch;
}

namespace react {
namespace uwp {
struct WebSource {
  std::string uri;
  bool packagerAsset;
};
} // namespace uwp
} // namespace react

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation::Collections;
} // namespace winrt

namespace react {
namespace uwp {

struct WebViewEvents {
  static constexpr std::string_view OnLoadingStart = "onLoadingStart";
  static constexpr std::string_view OnLoadingFinish = "onLoadingFinish";
  static constexpr std::string_view OnMessage = "onMessage";
  static constexpr std::string_view OnLoadingError = "onLoadingError";
  static constexpr std::string_view OnHttpError = "onHttpError";
  static constexpr std::string_view OnShouldStartLoadWithRequest = "onShouldStartLoadWithRequest";
};

enum class WebViewCommands : int32_t { InjectJavaScript = 0, GoBack = 1, GoForward = 2, Reload = 3, StopLoading = 4 };

class WebView : public react::uwp::ShadowNodeBase {
  using Super = react::uwp::ShadowNodeBase;

 public:
  WebView();
  ~WebView();

  void createView() override;

  // Handle commands received from ReactNative
  void dispatchCommand(int64_t commandId, const folly::dynamic &commandArgs) override;

  // Handle property changes from ReactNative
  void updateProperties(const folly::dynamic &&props) override;

 private:
  winrt::IAsyncAction injectJavaScript(winrt::WebView webView, std::string javaScript);
  winrt::IAsyncAction injectMessageJavaScript(winrt::WebView webView);
  folly::dynamic createWebViewArgs(winrt::WebView view);

  winrt::event_token m_onMessageToken;
  winrt::event_token m_onLoadedToken;
  winrt::event_token m_onLoadStartToken;
  winrt::event_token m_onErrorToken;
  std::string m_injectedJavascript;
};
} // namespace uwp
} // namespace react
