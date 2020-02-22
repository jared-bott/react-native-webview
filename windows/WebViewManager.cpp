// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "pch.h"

#include "WebViewManager.h"

#include <Views/ShadowNodeBase.h>

#include <Utils/ValueUtils.h>

#include <IReactInstance.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>

#include <condition_variable>
#include <mutex>
#include <type_traits>

using namespace folly;

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Foundation::Collections;
} // namespace winrt

namespace react {
namespace uwp {

WebViewManager::WebViewManager(const std::shared_ptr<IReactInstance> &reactInstance) : Super(reactInstance) {
  SetupPropertyHandlersInternal();
}

const char *WebViewManager::GetName() const {
  return "RNCWebView";
}

XamlView WebViewManager::CreateViewCore(int64_t tag) {
#ifdef CHAKRACORE_UWP
  return winrt::WebView(winrt::WebViewExecutionMode::SeparateProcess);
#else
  return winrt::WebView();
#endif
}

void WebViewManager::UpdateProperties(ShadowNodeBase *nodeToUpdate, const folly::dynamic &reactDiffMap) {
  XamlView view = nodeToUpdate->GetView();
  UpdatePropertiesInternal(view, reactDiffMap);
  Super::UpdateProperties(nodeToUpdate, reactDiffMap);
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

folly::dynamic WebViewManager::GetExportedCustomDirectEventTypeConstants() const {
  auto directEvents = Super::GetExportedCustomDirectEventTypeConstants();
  directEvents[WebViewEvents::OnLoadingStart] = folly::dynamic::object("registrationName", "onLoadingStart");
  directEvents[WebViewEvents::OnLoadingFinish] = folly::dynamic::object("registrationName", "onLoadingFinish");
  directEvents[WebViewEvents::OnMessage] = folly::dynamic::object("registrationName", "onMessage");
  directEvents[WebViewEvents::OnLoadingError] = folly::dynamic::object("registrationName", "onLoadingError");
  directEvents[WebViewEvents::OnHttpError] = folly::dynamic::object("registrationName", "onHttpError");
  directEvents[WebViewEvents::OnShouldStartLoadWithRequest] =
      folly::dynamic::object("registrationName", "onShouldStartLoadWithRequest");

  return directEvents;
}

folly::dynamic WebViewManager::GetNativeProps() const {
  auto props = Super::GetNativeProps();
  // TODO: implement native props propagation from property map
  props.update(folly::dynamic::object("source", "Map") //
               ("injectedJavaScript", "string") //
  );

  return props;
}

folly::dynamic WebViewManager::GetCommands() const {
  dynamic command = dynamic::object;
  command["injectJavaScript"] = static_cast<int32_t>(WebViewCommands::InjectJavaScript);
  command["goBack"] = static_cast<int32_t>(WebViewCommands::GoBack);
  command["goForward"] = static_cast<int32_t>(WebViewCommands::GoForward);
  command["reload"] = static_cast<int32_t>(WebViewCommands::Reload);
  command["stopLoading"] = static_cast<int32_t>(WebViewCommands::StopLoading);
  return command;
}

facebook::react::ShadowNode *WebViewManager::createShadow() const {
  return new WebView();
}

void WebViewManager::DispatchCommand(XamlView viewToUpdate, int64_t commandId, const folly::dynamic &commandArgs) {
  auto view = viewToUpdate.as<winrt::WebView>();

  if (view == nullptr) {
    return;
  }
}

void WebViewManager::FireEvent(const std::string &eventName, folly::dynamic eventArgs) {
  const auto instance = GetReactInstance().lock();
  if (instance) {
    folly::dynamic params = folly::dynamic::array("WebView", eventName, eventArgs);
    instance->CallJsFunction("RCTEventEmitter", "receiveEvent", std::move(params));
  }
}

WebView::WebView() {}

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

void WebView::createView() {
  Super::createView();

  auto webView = GetView().as<winrt::WebView>();
  auto weakInstance = GetViewManager()->GetReactInstance();

  m_onMessageToken = webView.ScriptNotify([=](const winrt::IInspectable, const winrt::NotifyEventArgs args) {
    auto instance = weakInstance.lock();
    if (!m_updating && instance != nullptr) {
      folly::dynamic data = dynamic::object("data", winrt::to_string(args.Value()));
      instance->DispatchEvent(m_tag, WebViewEvents::OnMessage.data(), std::move(data));
    }
  });

  m_onLoadStartToken =
      webView.NavigationStarting([=](winrt::WebView view, winrt::WebViewNavigationStartingEventArgs args) {
        auto instance = weakInstance.lock();
        // On initial load, this will be called before props are finished updating.
        if (instance != nullptr) {
          {
            // TODO: Can any of these be supported? lockIdentifier mainDocumentURL(iOS only) navigationType
            auto loadArgs = createWebViewArgs(view);
            // This is what Android does for navigation type.
            loadArgs["navigationType"] = "other";
            // Can't get the whole thing to work correctly. Commenting out the event.
            // instance->DispatchEvent(m_tag, WebViewEvents::OnShouldStartLoadWithRequest.data(), std::move(loadArgs));
            args.Cancel(true);
          }
          instance->DispatchEvent(m_tag, WebViewEvents::OnLoadingStart.data(), createWebViewArgs(view));
        }
      });

  m_onLoadedToken =
      webView.NavigationCompleted([=](winrt::WebView view, winrt::WebViewNavigationCompletedEventArgs args) {
        auto instance = weakInstance.lock();
        if (!m_updating && instance != nullptr) {
          injectMessageJavaScript(view);
          instance->DispatchEvent(m_tag, WebViewEvents::OnLoadingFinish.data(), createWebViewArgs(view));
        }
      });

  m_onErrorToken =
      webView.NavigationFailed([=](winrt::IInspectable view, winrt::WebViewNavigationFailedEventArgs args) {
        auto instance = weakInstance.lock();
        if (!m_updating && instance != nullptr) {
          winrt::WebView webView = view.as<winrt::WebView>();
          if (webView) {
            auto httpCode = static_cast<int32_t>(args.WebErrorStatus());
            {
              auto errorArgs = createWebViewArgs(webView);
              // TODO: The args need additional fields.
              // code description didFailProvisionalNavigation domain
              errorArgs["code"] = httpCode;

              instance->DispatchEvent(m_tag, WebViewEvents::OnLoadingError.data(), std::move(errorArgs));
            }
            {
              // statusCode
              auto errorArgs = createWebViewArgs(webView);
              errorArgs["statusCode"] = httpCode;
              instance->DispatchEvent(m_tag, WebViewEvents::OnHttpError.data(), std::move(errorArgs));
            }
          }
        }
      });
}

void WebView::dispatchCommand(int64_t commandId, const folly::dynamic &commandArgs) {
  auto webView = GetView().as<winrt::WebView>();

  switch (commandId) {
    case static_cast<int64_t>(WebViewCommands::InjectJavaScript):
      injectJavaScript(webView, commandArgs[0].asString());
      break;
    case static_cast<int64_t>(WebViewCommands::GoBack):
      if (webView.CanGoBack()) {
        webView.GoBack();
      }
      break;
    case static_cast<int64_t>(WebViewCommands::GoForward):
      if (webView.CanGoForward()) {
        webView.GoForward();
      }
      break;
    case static_cast<int64_t>(WebViewCommands::Reload):
      webView.Refresh();
      break;
    case static_cast<int64_t>(WebViewCommands::StopLoading):
      webView.Stop();
      break;
  }
}

void WebView::updateProperties(const folly::dynamic &&props) {
  m_updating = true;

  auto webView = GetView().as<winrt::WebView>();
  if (webView == nullptr) {
    return;
  }

  for (auto &pair : props.items()) {
    const std::string &propertyName = pair.first.getString();
    const folly::dynamic &propertyValue = pair.second;

    if (propertyName == "injectedJavaScript") {
      if (propertyValue.isString()) {
        m_injectedJavascript = propertyValue.asString();
      } else {
        // TODO: Is this an error condition? Not sure what it sets for undefined
        m_injectedJavascript = std::string();
      }
    }
  }

  Super::updateProperties(std::move(props));
  m_updating = false;
}

winrt::IAsyncAction WebView::injectJavaScript(winrt::WebView webView, std::string javaScript) {
  try {
    winrt::hstring script = winrt::to_hstring(javaScript);
    auto result = co_await webView.InvokeScriptAsync(L"eval", {script});
  } catch (std::exception &e) {
    // I don't have a logger?
    OutputDebugStringA("\n");
    OutputDebugStringA(e.what());
  }
}

winrt::IAsyncAction WebView::injectMessageJavaScript(winrt::WebView webView) {
  // This is for onMessage
  co_await injectJavaScript(
      webView, "window.ReactNativeWebView = {postMessage: function (data) {window.external.notify(String(data));}};");
  if (!m_injectedJavascript.empty()) {
    co_await injectJavaScript(webView, m_injectedJavascript);
  }
}

folly::dynamic WebView::createWebViewArgs(winrt::WebView view) {
  folly::dynamic args = folly::dynamic::object;
  args["canGoBack"] = view.CanGoBack();
  args["canGoForward"] = view.CanGoForward();
  // Presumably if the view isn't loaded, then it is loading.
  args["loading"] = !view.IsLoaded();
  args["target"] = m_tag;
  args["title"] = winrt::to_string(view.DocumentTitle());
  args["url"] = winrt::to_string(view.Source().AbsoluteCanonicalUri());

  return args;
}
} // namespace uwp
} // namespace react
