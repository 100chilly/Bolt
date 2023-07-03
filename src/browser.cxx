#include "browser.hxx"
#include "include/views/cef_window.h"

#include <algorithm>

#include <fmt/core.h>

Browser::Window::Window(CefRefPtr<CefClient> client, Browser::Details details, CefString url, bool show_devtools_for_children):
	show_devtools_for_children(show_devtools_for_children), details(details), window(nullptr), browser_view(nullptr), pending_child(nullptr)
{
	fmt::print("[B] Browser::Window constructor, this={}\n", reinterpret_cast<uintptr_t>(this));
	CefBrowserSettings browser_settings;
	browser_settings.background_color = CefColorSetARGB(0, 0, 0, 0);
	if (details.controls_overlay) {
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		dict->SetString("BoltAppUrl", url);
		this->browser_view = CefBrowserView::CreateBrowserView(client, "https://bolt-internal/frame.html", browser_settings, std::move(dict), nullptr, this);
	} else {
		this->browser_view = CefBrowserView::CreateBrowserView(client, url, browser_settings, nullptr, nullptr, this);
	}
	CefWindow::CreateTopLevelWindow(this);

}

Browser::Window::Window(Browser::Details details, bool show_devtools_for_children):
	show_devtools_for_children(show_devtools_for_children), details(details), window(nullptr), browser_view(nullptr), pending_child(nullptr)
{
	fmt::print("[B] Browser::Window popup constructor, this={}\n", reinterpret_cast<uintptr_t>(this));
}

void Browser::Window::OnWindowCreated(CefRefPtr<CefWindow> window) {
	fmt::print("[B] OnWindowCreated {} this={}\n", window->GetID(), reinterpret_cast<uintptr_t>(this));
	this->window = std::move(window);
	this->window->AddChildView(this->browser_view);
	if (this->details.center_on_open) {
		this->window->CenterWindow(CefSize(this->details.preferred_width, this->details.preferred_height));
	}
	this->window->Show();
}

void Browser::Window::OnWindowClosing(CefRefPtr<CefWindow> window) {
	fmt::print("[B] OnWindowClosing {} this={}\n", window->GetID(), reinterpret_cast<uintptr_t>(this));
	this->window = nullptr;
	this->browser_view = nullptr;
	this->pending_child = nullptr;
}

CefRect Browser::Window::GetInitialBounds(CefRefPtr<CefWindow> window) {
	return cef_rect_t {
		.x = this->details.startx,
		.y = this->details.starty,
		.width = this->details.preferred_width,
		.height = this->details.preferred_height,
	};
}

cef_show_state_t Browser::Window::GetInitialShowState(CefRefPtr<CefWindow>) {
	return CEF_SHOW_STATE_NORMAL;
}

bool Browser::Window::IsFrameless(CefRefPtr<CefWindow>) {
	return !this->details.frame;
}

bool Browser::Window::CanResize(CefRefPtr<CefWindow>) {
	return this->details.resizeable;
}

bool Browser::Window::CanMaximize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::Window::CanMinimize(CefRefPtr<CefWindow>) {
	return false;
}

bool Browser::Window::CanClose(CefRefPtr<CefWindow> win) {
	fmt::print("[B] CanClose for window {}, this={}\n", window->GetID(), reinterpret_cast<uintptr_t>(this));

	// Empty this->children by swapping it with a local empty vector, then iterate the local vector
	std::vector<CefRefPtr<Browser::Window>> children;
	std::swap(this->children, children);
	for (CefRefPtr<Window>& window: children) {
		window->browser_view->GetBrowser()->GetHost()->CloseBrowser(true);
	}

	// CEF will call CefLifeSpanHandler::DoClose (implemented in Client), giving us a chance to
	// do cleanup and then call TryCloseBrowser() a second time.
	// This strategy is suggested by official examples, e.g. cefsimple:
	// https://github.com/chromiumembedded/cef/blob/5735/tests/cefsimple/simple_app.cc#L38-L45
	CefRefPtr<CefBrowser> browser = this->browser_view->GetBrowser();
	if (browser) {
		return browser->GetHost()->TryCloseBrowser();
	} else {
		return true;
	}
}

CefSize Browser::Window::GetPreferredSize(CefRefPtr<CefView>) {
	return CefSize(this->details.preferred_width, this->details.preferred_height);
}

CefRefPtr<CefBrowserViewDelegate> Browser::Window::GetDelegateForPopupBrowserView(CefRefPtr<CefBrowserView>, const CefBrowserSettings&, CefRefPtr<CefClient>, bool is_devtools) {
	fmt::print("[B] GetDelegateForPopupBrowserView this={}\n", reinterpret_cast<uintptr_t>(this));
	Browser::Details details = {
		.preferred_width = this->popup_features.widthSet ? this->popup_features.width : 800,
		.preferred_height = this->popup_features.heightSet ? this->popup_features.height : 608,
		.startx = this->popup_features.x,
		.starty = this->popup_features.y,
		.center_on_open = !this->popup_features.xSet || !this->popup_features.ySet,
		.resizeable = true,
		.frame = true,
		.controls_overlay = false,
		.is_devtools = is_devtools,
	};
	this->pending_child = new Browser::Window(details, this->show_devtools_for_children);
	return this->pending_child;
}

bool Browser::Window::OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view, CefRefPtr<CefBrowserView> popup_browser_view, bool is_devtools) {
	fmt::print("[B] OnPopupBrowserViewCreated this={}\n", reinterpret_cast<uintptr_t>(this));
	this->pending_child->browser_view = popup_browser_view;
	CefWindow::CreateTopLevelWindow(this->pending_child);
	this->children.push_back(this->pending_child);
	CefRefPtr<Browser::Window> child = nullptr;
	std::swap(this->pending_child, child);
	if (this->show_devtools_for_children && !is_devtools) {
		child->ShowDevTools();
	}
	return true;
}

void Browser::Window::OnBrowserCreated(CefRefPtr<CefBrowserView>, CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] OnBrowserCreated this={} {}\n", reinterpret_cast<uintptr_t>(this), browser->GetIdentifier());
}

cef_chrome_toolbar_type_t Browser::Window::GetChromeToolbarType() {
	return CEF_CTT_NONE;
}

bool Browser::Window::CloseBrowser(CefRefPtr<CefBrowser> browser) {
	fmt::print("[B] CloseBrowser this={}\n", reinterpret_cast<uintptr_t>(this));
	this->children.erase(
		std::remove_if(
			this->children.begin(),
			this->children.end(),
			[&browser](const CefRefPtr<Browser::Window>& window){ return window->CloseBrowser(browser); }
		),
		this->children.end()
	);
	return this->browser_view->GetBrowser()->IsSame(browser);
	fmt::print("[B] CloseBrowser END this={}\n", reinterpret_cast<uintptr_t>(this));
}

void Browser::Window::SetPopupFeaturesForBrowser(CefRefPtr<CefBrowser> browser, const CefPopupFeatures& popup_features) {
	for (CefRefPtr<Window>& window: this->children) {
		window->SetPopupFeaturesForBrowser(browser, popup_features);
	}
	if (this->browser_view->GetBrowser()->IsSame(browser)) {
		this->popup_features = popup_features;
	}
}

void Browser::Window::ShowDevTools() {
	CefRefPtr<CefBrowserHost> browser_host = browser_view->GetBrowser()->GetHost();
	CefWindowInfo window_info; // ignored, because this is a BrowserView
	CefBrowserSettings browser_settings;
	browser_host->ShowDevTools(window_info, browser_host->GetClient(), browser_settings, CefPoint());
}
