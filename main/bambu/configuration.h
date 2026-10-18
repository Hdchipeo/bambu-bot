#pragma once

// Generated local file is ignored by Git; the demo needs no printer credentials.
#if !defined(BAMBU_FORCE_DEMO) && __has_include("local_config.h")
#include "local_config.h"
#else
#define BAMBU_DEMO true
#define BAMBU_HOST ""
#define BAMBU_SERIAL ""
#define BAMBU_ACCESS_CODE ""
#define BAMBU_TLS_CA_PEM ""
#define BAMBU_OTA_URL ""
#endif

#ifndef BAMBU_VOICE_NOTIFICATIONS
#define BAMBU_VOICE_NOTIFICATIONS true
#endif
#ifndef BAMBU_WIKI_URL
#define BAMBU_WIKI_URL ""
#endif
#ifndef BAMBU_WIKI_TOKEN
#define BAMBU_WIKI_TOKEN ""
#endif
