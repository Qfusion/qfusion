#pragma once

#define ANGELWRAP_API_VERSION 16

struct angelwrap_api_s;

void QAS_Init();
struct angelwrap_api_s *QAS_GetAngelExport();
void QAS_Shutdown();
