/*
 * ============================================================================
 * LWIP OPTIONS - CONFIGURATION FOR THE NETWORKING LIBRARY
 * ============================================================================
 *
 * WHAT IS LWIP?
 * lwIP ("light-weight IP") is the networking software this project uses. It
 * implements the actual rules of how devices talk over Wi-Fi/Ethernet
 * networks - things like TCP, the protocol web browsers and servers use to
 * reliably exchange data, and DHCP, the protocol that automatically gets us
 * an IP address from the Wi-Fi router. We do not have to write any of that
 * ourselves; lwIP is a well-tested library made for exactly this kind of
 * small embedded device.
 *
 * WHAT DOES THIS FILE DO?
 * lwIP has a huge number of optional features and tunable numbers (buffer
 * sizes, how many connections it can track at once, etc.), and it expects
 * the project using it to provide a file named exactly "lwipopts.h" that
 * chooses values for the ones that matter. Most of the settings below
 * are simply the standard starting point recommended by the Pico SDK's own
 * examples for a Wi-Fi project like this one - they are sensible defaults,
 * not something specific to this project, and most of this file is left
 * unchanged from that starting point. The handful of settings that WERE
 * chosen or changed specifically for this project have their own comments
 * explaining why, further down (particularly the "HTTP server features"
 * section, and the memory size).
 *
 * Most of the individual "#define SOMETHING 1" or "#define SOMETHING 0"
 * lines below are simple on/off switches: 1 means "turn this feature on",
 * 0 means "turn this feature off". Where a name is not self-explanatory,
 * a short comment explains what it controls.
 */

#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H


// Common settings used in most of the pico_w examples
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html for details)

// allow override in some examples
#ifndef NO_SYS
/* NO_SYS=0 means "lwIP should use an underlying operating system" - here,
 * FreeRTOS - to run its own background processing as proper tasks, rather
 * than expecting the whole program to just be one big loop calling into
 * lwIP repeatedly. This project relies on that, since FreeRTOS is already
 * running everything else too. */
#define NO_SYS                      0
#endif
// allow override in some examples
#ifndef LWIP_SOCKET
/* Turns on the traditional "Berkeley sockets" API (connect/send/recv, the
 * same style used on Linux, Windows, etc.) as an alternative way to use
 * lwIP. This project does not actually use that particular API directly -
 * it uses lwIP's own httpd web-server library instead - but this option is
 * left on to match the standard Pico W networking starting point. */
#define LWIP_SOCKET                 1
#endif
#if PICO_CYW43_ARCH_POLL
#define MEM_LIBC_MALLOC             1
#else
// MEM_LIBC_MALLOC is incompatible with non polling versions
#define MEM_LIBC_MALLOC             0
#endif
/* How memory addresses inside lwIP's internal memory pool must be aligned
 * to, in bytes. 4 matches this chip's natural word size (it is a 32-bit
 * processor) and should not be changed. */
#define MEM_ALIGNMENT               4
#ifndef MEM_SIZE
/*
 * This is the total size, in bytes, of lwIP's own internal memory pool -
 * separate from the rest of the program's memory - that it carves pieces
 * out of whenever it needs to allocate something (for example, tracking a
 * new connection, or the ne SSI text buffer described below).
 *
 * Each SSI connection allocates a struct http_ssi_state holding a
 * tag_insert[LWIP_HTTPD_MAX_TAG_INSERT_LEN + 1] buffer from this heap, so
 * bump past the default 4000 bytes to leave room for that plus lwIP's own
 * allocations across a couple of concurrent connections. (See
 * LWIP_HTTPD_MAX_TAG_INSERT_LEN further down for what that buffer is for.)
 */
#define MEM_SIZE                    8000
#endif
/* How many outstanding "unacknowledged data" segments TCP can track across
 * all connections at once. */
#define MEMP_NUM_TCP_SEG            32
/* How many outgoing packets can be queued up waiting for an ARP reply (ARP
 * is the low-level protocol used to translate an IP address into a
 * physical network hardware address). */
#define MEMP_NUM_ARP_QUEUE          10
/* How many network packet buffers ("pbufs") lwIP keeps in its pool,
 * ready to be handed out whenever a packet needs to be built or received. */
#define PBUF_POOL_SIZE              24
/* The next several LWIP_* = 1 lines each turn on one whole protocol or
 * feature that this project needs: ARP (translating IP addresses to
 * hardware addresses), Ethernet framing, ICMP ("ping"), raw sockets,
 * DHCP (automatically getting an IP address from the router), IPv4, TCP,
 * UDP, and DNS (turning a hostname into an IP address). */
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
/* TCP_MSS is the "maximum segment size" - the largest chunk of data TCP
 * will put in a single network packet. TCP_WND and TCP_SND_BUF are the
 * receive/send "window" sizes (how much data can be in flight, unacknowledged,
 * at once) - both set here as a multiple of TCP_MSS, a common way to size
 * them relative to the packet size actually being used. TCP_SND_QUEUELEN
 * is worked out from those so the send queue is always big enough to hold
 * a full window's worth of data. */
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
/* Lets our own code register a function to be told when the network
 * connection comes up/down, or when the physical link status changes. This
 * project does not currently use either callback, but leaves the ability
 * turned on, matching the standard starting point. */
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
/* Lets the device be given a network hostname (a friendly name some
 * routers show instead of just an IP address). */
#define LWIP_NETIF_HOSTNAME         1
/* NETCONN is yet another, different lwIP API style (sitting between raw
 * lwIP calls and full sockets) - turned off because this project does not
 * use it. */
#define LWIP_NETCONN                0
/* The next four turn off lwIP's built-in usage statistics tracking -
 * counting how many packets/memory blocks/etc. have been used - which
 * this project has no need for and which would use a small amount of
 * extra memory and CPU time to maintain if turned on. */
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0
// #define ETH_PAD_SIZE                2
/* Which built-in algorithm lwIP uses to compute network checksums (a small
 * number added to each packet so the receiver can detect if it was
 * corrupted in transit). Algorithm "3" is a reasonably fast, generic
 * implementation suitable for this kind of chip. */
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
/* Lets TCP connections use "keepalive" - periodically checking that the
 * other end (the browser) is still there, even if no real data is being
 * sent, so a connection that silently disappeared (for example, a laptop
 * put to sleep) eventually gets noticed and cleaned up. */
#define LWIP_TCP_KEEPALIVE          1
/* A small performance/memory-usage tweak: prefer sending each outgoing
 * packet as a single contiguous buffer rather than possibly chained
 * pieces, which the underlying Wi-Fi driver on this chip prefers. */
#define LWIP_NETIF_TX_SINGLE_PBUF   1
/* Skip two extra address-conflict-detection checks during DHCP (asking
 * "is anyone else already using this address?") to make getting an IP
 * address a little faster; not essential on a typical home network. */
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

/*
 * NDEBUG ("no debug") is a standard C convention: build tools define it
 * automatically for a "release" (optimized, production) build, and leave
 * it undefined for a "debug" build. When it is NOT defined (a debug
 * build), this turns on lwIP's own internal debug logging and statistics
 * printing. This project builds in Release mode by default (see
 * CMakeLists.txt), so in practice this block is normally switched off.
 */
#ifndef NDEBUG
#define LWIP_DEBUG                  1
#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          1
#endif

/*
 * Even when the block above turns debug logging ON overall, lwIP still
 * lets you choose which SUBSYSTEMS actually print anything, to avoid being
 * flooded with messages about parts you do not care about. Every line
 * below sets one subsystem's logging to LWIP_DBG_OFF - meaning even in a
 * debug build, none of lwIP's internal subsystems print debug output. Our
 * own project code still prints its own messages just fine (see, for
 * example, temperature_receiver.c) - this only affects lwIP's internal
 * logging.
 */
#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define PPP_DEBUG                   LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF

/*
 * A "PCB" (protocol control block) is the internal bookkeeping structure
 * lwIP keeps for each TCP connection. This is one of the few settings
 * chosen specifically for THIS project rather than copied from the
 * standard example: a web browser commonly opens several connections to
 * the same server at once (for example, one for the page itself and
 * separate ones for the chart data it fetches), so we reserve enough PCBs
 * to comfortably cover that.
 */
#define MEMP_NUM_TCP_PCB                  12

/*
 * HTTP server features. These three lines turn on the specific pieces of
 * lwIP's built-in "httpd" web server that this project relies on:
 *
 *   LWIP_HTTPD_CGI - lets a URL (like "/request-temperature") run our own
 *       C function instead of just returning a fixed file. See
 *       web/web_server.c for how this project uses it.
 *
 *   LWIP_HTTPD_SSI - lets a placeholder INSIDE a file (like
 *       "<!--#temphist-->") be filled in with fresh data on every request.
 *       See web/temperature_ssi.c for how this project uses it.
 *
 *   LWIP_HTTPD_SSI_MULTIPART - an optional extra feature of SSI that would
 *       let a single placeholder's replacement text be built up across
 *       several smaller pieces, for cases too big to fit in one buffer.
 *       Turned off (0) because LWIP_HTTPD_MAX_TAG_INSERT_LEN below is
 *       already made large enough to hold the whole reply in one piece.
 */
#define LWIP_HTTPD_CGI                    1
#define LWIP_HTTPD_SSI                    1
#define LWIP_HTTPD_SSI_MULTIPART          0

/*
 * Replace <!--#tag--> with its inserted value instead of leaving the
 * raw tag text in the output. Required for SSI tags used inside
 * <script> blocks, where "<!--" is not a valid JS comment opener.
 *
 * (In detail: lwIP's default behavior is to leave the ORIGINAL placeholder
 * text in the output and simply add the replacement value right after it -
 * fine for plain HTML text, but broken for our use case, since the
 * placeholder lives inside a JavaScript <script> block in
 * network/http/index.shtml where the leftover "<!--" text is not valid
 * JavaScript syntax. Setting this to 0 makes lwIP swap the placeholder OUT
 * for the value instead of leaving both.)
 */
#define LWIP_HTTPD_SSI_INCLUDE_TAG        0


/* The longest a placeholder NAME (like "temphist") is allowed to be. */
#define LWIP_HTTPD_MAX_TAG_NAME_LEN       16

/*
 * Large enough for {"t":[...],"v":[...]} holding TEMPERATURE_STORE_CAPACITY
 * (48) readings: 48 timestamps (up to 10 digits) + 48 "-327.67" worst-case
 * values, plus separators ~= 950 bytes.
 *
 * This is the size of the scratch buffer lwIP hands to
 * temperature_ssi.c's callback function to write the replacement text
 * into (see insert_buffer in web/temperature_ssi.c) - it has to be big
 * enough to hold the LARGEST possible reply, which is the full 48-reading
 * history, all at once (since LWIP_HTTPD_SSI_MULTIPART is off above).
 */
#define LWIP_HTTPD_MAX_TAG_INSERT_LEN     1024

#if !NO_SYS
/* How much stack memory (scratch space) lwIP's own internal background
 * tasks get. TCPIP_THREAD_STACKSIZE is for lwIP's main processing task;
 * DEFAULT_THREAD_STACKSIZE is the fallback size for any other lwIP-created
 * task. */
#define TCPIP_THREAD_STACKSIZE            1024
#define DEFAULT_THREAD_STACKSIZE          2048
/* How many messages can be queued up waiting to be delivered to a "raw"
 * network connection, and to lwIP's main processing task, before further
 * messages have to wait. */
#define DEFAULT_RAW_RECVMBOX_SIZE         8
#define TCPIP_MBOX_SIZE                   8
#define LWIP_TIMEVAL_PRIVATE              0

// not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT     1

// ping_thread sets socket receive timeout, so enable this feature
#define LWIP_SO_RCVTIMEO                  1
#endif


/*
 * Web content generated by pico_set_lwip_httpd_content().
 *
 * This names the file that the Pico SDK's build step generates containing
 * all of this project's web page content (network/http/*.html, *.json,
 * *.shtml), turned into raw bytes baked directly into the compiled
 * program. lwIP's httpd server reads from that generated file instead of
 * a real filesystem - this tiny microcontroller has no disk to read files
 * from at runtime, so everything the web server can serve has to already
 * be built into the program ahead of time. See CMakeLists.txt for the
 * pico_set_lwip_httpd_content() call that generates it, and
 * build/generated/pico_fsdata.inc for the actual generated file once the
 * project has been built.
 */
#define HTTPD_FSDATA_FILE                 "pico_fsdata.inc"

#endif /* __LWIPOPTS_H__ */
