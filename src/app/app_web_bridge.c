#if defined(PLATFORM_WEB)
#include <emscripten.h>

EM_JS(int, web_extension_host_js, (void), {
    if(typeof window === 'undefined')
        return 0;
    if(window.__inbeExtension)
        return 1;
    return (window.location && window.location.protocol === 'chrome-extension:' &&
            typeof chrome !== 'undefined' && chrome.runtime) ? 1 : 0;
});

EM_JS(void, web_extension_break_now_js, (int break_type), {
    if (typeof window !== 'undefined' &&
        typeof window.__inbeExtensionBreakNow === 'function') {
        window.__inbeExtensionBreakNow(break_type);
    }
});

#endif
