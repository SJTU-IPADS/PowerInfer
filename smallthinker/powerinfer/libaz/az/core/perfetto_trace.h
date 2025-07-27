#ifdef __cplusplus
extern "C" {
#endif
#if defined(AZ_ENABLE_PERFETTO)
void TraceEventStart(const char *name);
void TraceEventEnd(const char *name);
#endif
#ifdef __cplusplus
}
#endif