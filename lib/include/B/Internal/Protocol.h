#ifndef PROTOCOL_H_2DC53D94_DDEB_472B_99F0_6AAD07BC901B
#define PROTOCOL_H_2DC53D94_DDEB_472B_99F0_6AAD07BC901B

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnswerVTable;
struct B_AnyAnswer;
struct B_AnyQuestion;
struct B_Broker;
struct B_Exception;
struct B_Identity;
struct B_QuestionVTable;
struct B_UUID;

enum B_WorkerCommand {
    B_WORKER_READY = 1,
    B_WORKER_DONE_AND_READY = 2,
};

struct B_RequestID {
    uint8_t bytes[4];
};

bool
b_protocol_client_endpoint(
    char *buffer,
    size_t buffer_size,
    struct B_Broker const *broker);

bool
b_protocol_worker_endpoint(
    char *buffer,
    size_t buffer_size,
    struct B_Broker const *broker);

void
b_protocol_send_uuid(
    void *socket_zmq,
    struct B_UUID,
    int flags,
    struct B_Exception **);

struct B_UUID
b_protocol_recv_uuid(
    void *socket_zmq,
    int flags,
    struct B_Exception **);

void
b_protocol_send_question(
    void *socket_zmq,
    struct B_AnyQuestion const *,
    struct B_QuestionVTable const *,
    int flags,
    struct B_Exception **);

struct B_AnyQuestion *
b_protocol_recv_question(
    void *socket_zmq,
    struct B_QuestionVTable const *,
    int flags,
    struct B_Exception **);

void
b_protocol_send_answer(
    void *socket_zmq,
    struct B_AnyAnswer const *,
    struct B_AnswerVTable const *,
    int flags,
    struct B_Exception **);

struct B_AnyAnswer *
b_protocol_recv_answer(
    void *socket_zmq,
    struct B_AnswerVTable const *,
    int flags,
    struct B_Exception **);

void
b_protocol_send_identity_envelope(
    void *socket_zmq,
    struct B_Identity const *,
    int flags,
    struct B_Exception **);

struct B_Identity *
b_protocol_recv_identity_envelope(
    void *socket_zmq,
    int flags,
    struct B_Exception **);

void
b_protocol_send_worker_command(
    void *socket_zmq,
    enum B_WorkerCommand,
    int flags,
    struct B_Exception **);

enum B_WorkerCommand
b_protocol_recv_worker_command(
    void *socket_zmq,
    int flags,
    struct B_Exception **);

B_ERRFUNC
b_protocol_send_request_id(
    void *socket_zmq,
    struct B_RequestID const *,
    int flags);

B_ERRFUNC
b_protocol_recv_request_id(
    void *socket_zmq,
    struct B_RequestID *,
    int flags);

#ifdef __cplusplus
}
#endif

#endif
