idf_component_register(
    SRCS "dual_clamp.c" "pool_scheduler.c" "stratum_recv_ctx.c"
    INCLUDE_DIRS "include"
    REQUIRES "tcp_transport"
)
