(function() {
    if (window.Lightning) return;

    const defaultConfig = {
        linkSelector: 'a:not([data-lightning="false"])',
        contentSelector: 'body',
        prefetch: true,
        prefetchDelay: 20,
        cacheTimeout: 0,
        scrollToTop: true,
        debug: false,
    };

    function Lightning(config = defaultConfig) {
        const options = Object.assign({}, defaultConfig, config || {});

        const instance = {
            generation: 0,
            pageCache: {},
        };

        Lightning.instance = instance;
        Lightning.VERSION = '0.0.1';

        const debug = (...args) => {
            if (options.debug) console.log(...args);
        };

        const getElement = (root) => {
            return root.querySelector(options.contentSelector);
        }

        const getState = () => {
            const el = getElement(document);
            const result = {
                html: el ? el.innerHTML : null,
                title: document.title,
                className: document.body.className,
            };
            return result;
        };

        const setState = (state) => {
            document.title = state.title;
            document.body.className = state.className;

            const el = getElement(document);
            if (el) {
                el.innerHTML = state.html;
            }

            LightningLinks();
        };

        const getPage = (path) => {
            return fetch(path).then((resp) => resp.text());
        };

        const getPageCached = (path) => {
            if (instance.pageCache[path]) {
                const cache = instance.pageCache[path];
                if (cache.promise) {
                    return cache.promise;
                }

                if (cache.data) {
                    if (options.cacheTimeout > 0)
                    {
                        const now = window.performance.now();
                        if (now - cache.time > options.cacheTimeout) {
                            cache.data = null;
                        }
                    }

                    if (cache.data) {
                        return Promise.resolve(cache.data);
                    }
                }
            }
                            
            const promise = getPage(path);

            instance.pageCache[path] = instance.pageCache[path] || {};
            instance.pageCache[path].promise = promise;

            return promise.then((text) => {
                const cache = instance.pageCache[path];
                cache.data = text;
                cache.time = window.performance.now();
                cache.promise = null;

                return text;
            });
        }

        function LightningLinks() {
            Array.from(document.querySelectorAll(options.linkSelector)).forEach((link) => {
                const origin = window.location.origin;
                const isExternal = !link.href.startsWith(origin);
                if (isExternal) return;
                if (link.lightning) return;

                link.lightning = true;

                const path = link.href.slice(origin.length);

                link.addEventListener('click', (e) => {
                    e.preventDefault();
                    e.stopPropagation();

                    debug("⚡");

                    const generation = ++instance.generation;

                    getPageCached(path).then((text) => {
                        if (instance.generation !== generation) return;

                        let html = null;

                        try {
                            const dom = new DOMParser();
                            html = dom.parseFromString(text, 'text/html');         
                        } catch (err) {
                            console.error(err);
                            window.location.href = link.href;
                            return;
                        }

                        if (html) {
                            const body = html.body;
                            const title = html.title;

                            const el = getElement(html);

                            if (el)
                            {
                                setState({
                                    title: html.title,
                                    html: el.innerHTML,
                                    className: body.className,
                                });

                                if (options.scrollToTop)
                                {
                                    window.scrollTo(0, 0);
                                }

                                const state = getState();
                                debug("push state", state);
                                window.history.pushState(state, title, path);
                            }
                            else
                            {
                                debug("Missing element from response!", path);
                            }
                        }
                    }).catch((err) => {
                        console.error(err);
                        window.location.href = link.href;
                    });

                    return false;
                });

                link.addEventListener('mouseenter', (e) => {
                    if (options.prefetch) {
                        link.lightningPrefetch = setTimeout(() => {
                            link.lightningPrefetch = null;

                            getPageCached(path);

                        }, options.prefetchDelay);
                    }
                });

                link.addEventListener('mouseleave', (e) => {
                    if (link.lightningPrefetch) {
                        clearTimeout(link.lightningPrefetch);
                        link.lightningPrefetch = null;
                    }
                });
            });
        }

        window.addEventListener('popstate', (event) => {
            const state = event.state;
            if (state) {
                debug("pop state", state);
                setState(state);
            }
        });

        const init = () => {
            const initialState = getState();
            window.history.pushState(initialState, initialState.title, '');

            LightningLinks();
        };

        if (document.readyState === 'complete') {
            init();
        } else {
            window.addEventListener('load', init);
        }
    };

    window.Lightning = Lightning;
})();

Lightning();