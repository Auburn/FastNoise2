// NOTE: This file creates a service worker that cross-origin-isolates the page (read more here: https://web.dev/coop-coep/) which allows us to use wasm threads.
// Normally you would set the COOP and COEP headers on the server to do this, but Github Pages doesn't allow this, so this is a hack to do that.

/* Edited version of: coi-serviceworker v0.1.6 - Guido Zuidhof, licensed under MIT */
// From here: https://github.com/gzuidhof/coi-serviceworker
if(typeof window === 'undefined') {
  self.addEventListener("install", () => self.skipWaiting());
  self.addEventListener("activate", e => e.waitUntil(self.clients.claim()));

  async function handleFetch(request) {
    if(request.cache === "only-if-cached" && request.mode !== "same-origin") {
      return;
    }
    
    if(request.mode === "no-cors") { // We need to set `credentials` to "omit" for no-cors requests, per this comment: https://bugs.chromium.org/p/chromium/issues/detail?id=1309901#c7
      request = new Request(request.url, {
        cache: request.cache,
        credentials: "omit",
        headers: request.headers,
        integrity: request.integrity,
        destination: request.destination,
        keepalive: request.keepalive,
        method: request.method,
        mode: request.mode,
        redirect: request.redirect,
        referrer: request.referrer,
        referrerPolicy: request.referrerPolicy,
        signal: request.signal,
      });
    }
    
    let r = await fetch(request).catch(e => console.error(e));
    
    if(r.status === 0) {
      return r;
    }

    const headers = new Headers(r.headers);
    headers.set("Cross-Origin-Embedder-Policy", "require-corp"); // or: credentialless
    headers.set("Cross-Origin-Opener-Policy", "same-origin");
    
    return new Response(r.body, { status: r.status, statusText: r.statusText, headers });
  }

  self.addEventListener("fetch", function(e) {
    e.respondWith(handleFetch(e.request)); // respondWith must be executed synchonously (but can be passed a Promise)
  });
  
} else {
  (async function() {
    // Reload at most once per attempt. The marker rides in the URL fragment: Chrome switches browsing
    // context group when COOP starts or stops applying, and across that switch a sessionStorage flag can
    // come back stale and history.state is dropped. The URL survives the reload in both directions.
    const marker = "#coi-reloaded";
    const reloadedBySelf = window.location.hash === marker;
    if(reloadedBySelf) {
      window.history.replaceState(window.history.state, "", window.location.pathname + window.location.search);
    }

    if(window.crossOriginIsolated !== false) return;

    if(!window.isSecureContext || !navigator.serviceWorker) {
      console.warn("COOP/COEP Service Worker unavailable: a secure context with service worker support is required.");
      return;
    }

    const scriptURL = window.document.currentScript.src;
    const registration = await navigator.serviceWorker.register(scriptURL).catch(e => console.error("COOP/COEP Service Worker failed to register:", e));
    if(!registration) return;
    console.log("COOP/COEP Service Worker registered", registration.scope);

    if(reloadedBySelf) {
      console.warn("Still not cross-origin isolated after reloading through the COOP/COEP Service Worker; the browser or embedding page is blocking isolation.");
      return;
    }

    // Wait until this worker (not merely any worker in the registration) is active, so the
    // reloaded navigation is served with COOP/COEP.
    await new Promise(resolve => {
      const check = () => { if(registration.active && registration.active.scriptURL === scriptURL) resolve(); };
      const track = worker => worker && worker.addEventListener("statechange", check);
      registration.addEventListener("updatefound", () => track(registration.installing));
      track(registration.installing);
      track(registration.waiting);
      check();
    });

    window.history.replaceState(window.history.state, "", marker);
    console.log("Reloading page to make use of COOP/COEP Service Worker.");
    window.location.reload();
  })();
}

// Code to deregister:
// let registrations = await navigator.serviceWorker.getRegistrations();
// for(let registration of registrations) {
//   await registration.unregister();
// }
