// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "pch.h"

#include "WebView.h"

#include <Utils/ValueUtils.h>

#include <IReactInstance.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

using namespace folly;

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation::Collections;
} // namespace winrt

namespace winrt::ReactNativeWebView::implementation {

WebView::WebView(Microsoft::ReactNative::IReactContext const &reactContext) : m_reactContext(reactContext) {
#ifdef CHAKRACORE_UWP
  m_webView = winrt::WebView(winrt::WebViewExecutionMode::SeparateProcess);
#else
  m_webView = winrt::WebView();
#endif

  m_uiDispatcher = CoreWindow::GetForCurrentThread().Dispatcher();

  m_onMessageToken =
      m_webView.ScriptNotify([ref = get_weak()](const winrt::IInspectable, const winrt::NotifyEventArgs args) {
        if (auto self = ref.get()) {
          runOnQueue([weak_this{get_weak()}, data{winrt::to_string(args.Value())}]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  WebViewEvents::OnMessage.data(),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    { WriteProperty(eventDataWriter, L"data", data); }
                    eventDataWriter.WriteObjectEnd();
                  });
            }
          });
        }
      });

  m_onLoadStartToken = m_webView.NavigationStarting(
      [ref = get_weak()](winrt::WebView view, winrt::WebViewNavigationStartingEventArgs args) {
        if (auto self = ref.get()) {
          runOnQueue([weak_this{get_weak()}]() {
            if (auto strong_this{weak_this.get()}) {
              // strong_this->m_reactContext.DispatchEvent(
              //     *strong_this,
              //     WebViewEvents::OnShouldStartLoadWithRequest.data(),
              //     [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
              //       eventDataWriter.WriteObjectBegin();
              //       createWebViewArgs(eventDataWriter);
              //       { WriteProperty(eventDataWriter, L"navigationType", L"other"); }
              //       eventDataWriter.WriteObjectEnd();
              //     });
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  WebViewEvents::OnLoadingStart.data(),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    createWebViewArgs(eventDataWriter);
                    eventDataWriter.WriteObjectEnd();
                  });
            }
          });
        }
      });

  m_onLoadedToken = m_webView.NavigationCompleted(
      [ref = get_weak()](winrt::WebView view, winrt::WebViewNavigationCompletedEventArgs args) {
        if (auto self = ref.get()) {
          injectMessageJavaScript(m_view);
          runOnQueue([weak_this{get_weak()}]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  WebViewEvents::OnLoadingFinish.data(),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    createWebViewArgs(eventDataWriter);
                    eventDataWriter.WriteObjectEnd();
                  });
            }
          });
        }
      });

  m_onErrorToken = m_webView.NavigationFailed(
      [ref = get_weak()](winrt::IInspectable view, winrt::WebViewNavigationFailedEventArgs args) {
        if (auto self = ref.get()) {
          auto httpCode = static_cast<int32_t>(args.WebErrorStatus());
          runOnQueue([weak_this{get_weak()}, httpCode]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  WebViewEvents::OnLoadingError.data(),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    { WriteProperty(eventDataWriter, L"code", httpCode); }
                    createWebViewArgs(eventDataWriter);
                    eventDataWriter.WriteObjectEnd();
                  });
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  WebViewEvents::OnHttpError.data(),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    { WriteProperty(eventDataWriter, L"statusCode", httpCode); }
                    createWebViewArgs(eventDataWriter);
                    eventDataWriter.WriteObjectEnd();
                  });
            }
          });
        }
      });
}

WebView::~WebView() {
  auto webView = GetView().as<winrt::WebView>();
  if (m_onMessageToken) {
    webView.ScriptNotify(m_onMessageToken);
  }
  if (m_onLoadedToken) {
    webView.NavigationCompleted(m_onLoadedToken);
  }
  if (m_onLoadStartToken) {
    webView.NavigationStarting(m_onLoadStartToken);
  }

  if (m_onErrorToken) {
    webView.NavigationFailed(m_onErrorToken);
  }
}

void WebView::Load() {
  m_view.Navigate(winrt::Uri(m_uri));
}

void WebView::Set_UriString(hstring const &uri) {
  m_uri = uri;
}

void WebView::Set_StartingJavaScript(hstring const &script) {
  m_injectedJavascript = script;
}

void WebView::InjectJavaScript(hstring const &script) {
  injectJavaScript(webView, script);
}

void WebView::GoBack() {
  if (m_view.CanGoBack()) {
    m_view.GoBack();
  }
}

void WebView::GoForward() {
  if (m_view.CanGoForward()) {
    m_view.GoForward();
  }
}

void WebView::Reload() {
  m_view.Reload();
}

void WebView::StopLoading() {
  m_view.Stop();
}

winrt::IAsyncAction WebView::injectJavaScript(winrt::hstring javaScript) {
  try {
    auto result = co_await m_view.InvokeScriptAsync(L"eval", {javaScript});
  } catch (std::exception &e) {
    // I don't have a logger?
    OutputDebugStringA("\n");
    OutputDebugStringA(e.what());
  }
}

winrt::IAsyncAction WebView::injectMessageJavaScript() {
  // This is for onMessage
  co_await injectJavaScript(
      m_view, "window.ReactNativeWebView = {postMessage: function (data) {window.external.notify(String(data));}};");
  if (!m_injectedJavascript.empty()) {
    co_await injectJavaScript(m_view, m_injectedJavascript);
  }
}

folly::dynamic WebView::createWebViewArgs(winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) {
  folly::dynamic args = folly::dynamic::object;
  args["canGoBack"] = m_view.CanGoBack();
  args["canGoForward"] = m_view.CanGoForward();
  // Presumably if the view isn't loaded, then it is loading.
  args["loading"] = !m_view.IsLoaded();
  args["target"] = m_tag;
  args["title"] = winrt::to_string(m_view.DocumentTitle());
  args["url"] = winrt::to_string(m_view.Source().AbsoluteCanonicalUri());

  return args;
}

void WebView::runOnQueue(std::function<void()> &&func) {
  m_uiDispatcher.RunAsync(
      winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [func = std::move(func)]() { func(); });
}
} // namespace winrt::ReactNativeWebView::implementation
