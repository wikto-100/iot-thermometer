/*
 * ============================================================================
 * TEMPERATURE SSI - PUBLIC INTERFACE
 * ============================================================================
 *
 * WHAT IS "SSI"?
 * SSI stands for "Server Side Include". It is a simple way to put a
 * placeholder INSIDE a file, and have the web server fill it in with fresh
 * data every time that file is requested - before sending it to the
 * browser. The placeholder looks like an HTML comment, for example
 * "<!--#temphist-->", and it lives inside network/http/temphist.json (take
 * a look at that file - it is almost empty, just that one placeholder).
 * Every time a browser asks for that file, lwIP notices the placeholder,
 * calls our code to ask "what goes here right now?", and stitches the
 * answer into the response. This is how the web page gets a fresh
 * temperature history on every request, even though the file's other bytes
 * (its surrounding structure) never change and can be baked into the
 * program at build time.
 */

#ifndef TEMPERATURE_SSI_H
#define TEMPERATURE_SSI_H

/**
 * @brief Register temperature-related SSI tags with lwIP HTTPD
 *
 * Call this once, before starting the web server, so lwIP knows to call
 * back into our code whenever it encounters the "temphist" placeholder
 * tag. See temperature_ssi.c for what actually gets filled in.
 */
void temperature_ssi_register(void);

#endif
