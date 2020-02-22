// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <Views/FrameworkElementViewManager.h>
#include <Views/ShadowNodeBase.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include "Utils/PropertyHandlerUtils.h"
#include "WebView.h"

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

template <>
struct json_type_traits<react::uwp::WebSource> {
  static react::uwp::WebSource parseJson(const folly::dynamic &json) {
    react::uwp::WebSource source;
    for (auto &item : json.items()) {
      if (item.first == "uri")
        source.uri = item.second.asString();
      if (item.first == "__packager_asset")
        source.packagerAsset = item.second.asBool();
    }
    return source;
  }
};

namespace react {
namespace uwp {

class WebViewManager : public FrameworkElementViewManager {
  using Super = FrameworkElementViewManager;

 public:
  RCT_BEGIN_PROPERTY_MAP(WebViewManager)
  RCT_PROPERTY("source", setSource)
  RCT_END_PROPERTY_MAP()

  WebViewManager(const std::shared_ptr<IReactInstance> &reactInstance);

  const char *GetName() const override;
  void UpdateProperties(ShadowNodeBase *nodeToUpdate, const folly::dynamic &reactDiffMap) override;

  folly::dynamic GetExportedCustomDirectEventTypeConstants() const override;
  folly::dynamic GetNativeProps() const override;

  folly::dynamic GetCommands() const override;

  void DispatchCommand(XamlView viewToUpdate, int64_t commandId, const folly::dynamic &commandArgs) override;
  void FireEvent(const std::string &eventName, folly::dynamic eventArgs = folly::dynamic::object());
  facebook::react::ShadowNode *createShadow() const;

 protected:
  XamlView CreateViewCore(int64_t tag) override;

 private:
  void setSource(XamlView viewToUpdate, const WebSource &sources);
};

} // namespace uwp
} // namespace react
