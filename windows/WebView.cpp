// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "pch.h"

#include "WebView.h"

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
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
      m_webView.ScriptNotify([this, ref = get_weak()](const winrt::IInspectable, const winrt::NotifyEventArgs args) {
        if (auto self = ref.get()) {
          runOnQueue([this, weak_this{get_weak()}, data{winrt::to_string(args.Value())}]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  winrt::to_hstring(WebViewEvents::OnMessage),
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
      [this, ref = get_weak()](winrt::WebView view, winrt::WebViewNavigationStartingEventArgs args) {
        if (auto self = ref.get()) {
          runOnQueue([this, weak_this{get_weak()}]() {
            if (auto strong_this{weak_this.get()}) {
              // strong_this->m_reactContext.DispatchEvent(
              //     *strong_this,
              //     winrt::to_hstring(WebViewEvents::OnShouldStartLoadWithRequest),
              //     [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
              //       eventDataWriter.WriteObjectBegin();
              //       createWebViewArgs(eventDataWriter);
              //       { WriteProperty(eventDataWriter, L"navigationType", L"other"); }
              //       eventDataWriter.WriteObjectEnd();
              //     });
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  winrt::to_hstring(WebViewEvents::OnLoadingStart),
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
      [this, ref = get_weak()](winrt::WebView view, winrt::WebViewNavigationCompletedEventArgs args) {
        if (auto self = ref.get()) {
          injectMessageJavaScript();
          runOnQueue([this, weak_this{get_weak()}]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  winrt::to_hstring(WebViewEvents::OnLoadingFinish),
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
      [this, ref = get_weak()](winrt::IInspectable view, winrt::WebViewNavigationFailedEventArgs args) {
        if (auto self = ref.get()) {
          auto httpCode = static_cast<int32_t>(args.WebErrorStatus());
          runOnQueue([this, weak_this{get_weak()}, httpCode]() {
            if (auto strong_this{weak_this.get()}) {
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  winrt::to_hstring(WebViewEvents::OnLoadingError),
                  [&](winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) noexcept {
                    eventDataWriter.WriteObjectBegin();
                    { WriteProperty(eventDataWriter, L"code", httpCode); }
                    createWebViewArgs(eventDataWriter);
                    eventDataWriter.WriteObjectEnd();
                  });
              strong_this->m_reactContext.DispatchEvent(
                  *strong_this,
                  winrt::to_hstring(WebViewEvents::OnHttpError),
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
  if (m_onMessageToken) {
    m_webView.ScriptNotify(m_onMessageToken);
  }
  if (m_onLoadedToken) {
    m_webView.NavigationCompleted(m_onLoadedToken);
  }
  if (m_onLoadStartToken) {
    m_webView.NavigationStarting(m_onLoadStartToken);
  }

  if (m_onErrorToken) {
    m_webView.NavigationFailed(m_onErrorToken);
  }
}

void WebView::Load() {
  m_webView.Navigate(winrt::Uri(m_uri));
}

void WebView::Set_UriString(hstring const &uri) {
  m_uri = uri;
}

void WebView::Set_StartingJavaScript(hstring const &script) {
  m_injectedJavascript = script;
}

void WebView::InjectJavaScript(hstring const &script) {
  injectJavaScript(script);
}

void WebView::GoBack() {
  if (m_webView.CanGoBack()) {
    m_webView.GoBack();
  }
}

void WebView::GoForward() {
  if (m_webView.CanGoForward()) {
    m_webView.GoForward();
  }
}

void WebView::Reload() {
  m_webView.Refresh();
}

void WebView::StopLoading() {
  m_webView.Stop();
}

winrt::IAsyncAction WebView::injectJavaScript(winrt::hstring javaScript) {
  try {
    auto result = co_await m_webView.InvokeScriptAsync(L"eval", {javaScript});
  } catch (std::exception &e) {
    // I don't have a logger?
    OutputDebugStringA("\n");
    OutputDebugStringA(e.what());
  }
}

winrt::IAsyncAction WebView::injectMessageJavaScript() {
  // This is for onMessage
  co_await injectJavaScript(
      L"window.ReactNativeWebView = {postMessage: function (data) {window.external.notify(String(data));}};");
  if (!m_injectedJavascript.empty()) {
    co_await injectJavaScript(m_injectedJavascript);
  }
}

void WebView::createWebViewArgs(winrt::Microsoft::ReactNative::IJSValueWriter const &eventDataWriter) {
  auto tag = m_webView.GetValue(winrt::FrameworkElement::TagProperty()).as<winrt::IPropertyValue>().GetInt64();
  WriteProperty(eventDataWriter, L"canGoBack", m_webView.CanGoBack());
  WriteProperty(eventDataWriter, L"canGoForward", m_webView.CanGoForward());
  // Presumably if the view isn't loaded, then it is loading.
  WriteProperty(eventDataWriter, L"loading", !m_webView.IsLoaded());
  WriteProperty(eventDataWriter, L"target", tag);
  WriteProperty(eventDataWriter, L"title", m_webView.DocumentTitle());
  WriteProperty(eventDataWriter, L"url", m_webView.Source().AbsoluteCanonicalUri());
}

void WebView::runOnQueue(std::function<void()> &&func) {
  m_uiDispatcher.RunAsync(
      winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [func = std::move(func)]() { func(); });
}
} // namespace winrt::ReactNativeWebView::implementation
