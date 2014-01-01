#include <B/Answer.h>
#include <B/Exception.h>
#include <B/Internal/Identity.h>
#include <B/Internal/Protocol.h>
#include <B/Internal/ZMQ.h>
#include <B/Question.h>
#include <B/Serialize.h>

#include <zmq.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
b_protocol_client_endpoint(
    char *buffer,
    size_t buffer_size,
    struct B_BrokerAddress const *broker_address) {

    int rc = snprintf(
        buffer,
        buffer_size,
        "inproc://b_client_%p",
        broker_address);
    return rc >= 0 && rc == strlen(buffer);
}

bool
b_protocol_worker_endpoint(
    char *buffer,
    size_t buffer_size,
    struct B_BrokerAddress const *broker_address) {

    int rc = snprintf(
        buffer,
        buffer_size,
        "inproc://b_worker_%p",
        broker_address);
    return rc >= 0 && rc == strlen(buffer);
}

B_ERRFUNC
b_protocol_connect_client(
    void *context_zmq,
    struct B_BrokerAddress const *broker_address,
    int socket_type,
    void **out_socket_zmq) {

    char endpoint_buffer[1024];
    bool ok = b_protocol_client_endpoint(
        endpoint_buffer,
        sizeof(endpoint_buffer),
        broker_address);
    assert(ok);

    void *socket;
    struct B_Exception *ex = b_zmq_socket_connect(
        context_zmq,
        socket_type,
        endpoint_buffer,
        &socket);
    if (ex) {
        return ex;
    }

    *out_socket_zmq = socket;
    return NULL;
}

B_ERRFUNC
b_protocol_connect_worker(
    void *context_zmq,
    struct B_BrokerAddress const *broker_address,
    int socket_type,
    void **out_socket_zmq) {

    char endpoint_buffer[1024];
    bool ok = b_protocol_worker_endpoint(
        endpoint_buffer,
        sizeof(endpoint_buffer),
        broker_address);
    assert(ok);

    void *socket;
    struct B_Exception *ex = b_zmq_socket_connect(
        context_zmq,
        socket_type,
        endpoint_buffer,
        &socket);
    if (ex) {
        return ex;
    }

    *out_socket_zmq = socket;
    return NULL;
}

void
b_protocol_send_uuid(
    void *socket_zmq,
    struct B_UUID uuid,
    int flags,
    struct B_Exception **ex) {

    *ex = b_zmq_send(
        socket_zmq,
        uuid.uuid,
        strlen(uuid.uuid),
        flags);
    if (*ex) {
        return;
    }
}

struct B_UUID
b_protocol_recv_uuid(
    void *socket_zmq,
    int flags,
    struct B_Exception **ex) {

    zmq_msg_t message;
    *ex = b_zmq_msg_init_recv(&message, socket_zmq, flags);
    if (*ex) {
        return B_UUID("");
    }

    size_t string_length = zmq_msg_size(&message);
    char *uuid_string = malloc(string_length + 1);
    memcpy(
        uuid_string,
        zmq_msg_data(&message),
        string_length);
    uuid_string[string_length] = '\0';
    b_zmq_msg_close(&message);
    struct B_UUID uuid
        = b_uuid_from_temp_string(uuid_string);
    free(uuid_string);
    return uuid;
}

void
b_protocol_send_question(
    void *socket_zmq,
    struct B_AnyQuestion const *question,
    struct B_QuestionVTable const *question_vtable,
    int flags,
    struct B_Exception **ex) {

    size_t size;
    char *question_buffer = b_serialize_to_memory(
        question,
        question_vtable->serialize,
        &size);
    *ex = b_zmq_send(
        socket_zmq,
        question_buffer,
        size,
        flags);
    free(question_buffer);
    if (*ex) {
        return;
    }
}

struct B_AnyQuestion *
b_protocol_recv_question(
    void *socket_zmq,
    struct B_QuestionVTable const *question_vtable,
    int flags,
    struct B_Exception **ex) {

    zmq_msg_t message;
    *ex = b_zmq_msg_init_recv(&message, socket_zmq, flags);
    if (*ex) {
        return NULL;
    }

    struct B_AnyQuestion *question
        = b_deserialize_from_memory0(
            zmq_msg_data(&message),
            zmq_msg_size(&message),
            question_vtable->deserialize);

    b_zmq_msg_close(&message);
    return question;
}

void
b_protocol_send_answer(
    void *socket_zmq,
    struct B_AnyAnswer const *answer,
    struct B_AnswerVTable const *answer_vtable,
    int flags,
    struct B_Exception **ex) {

    size_t size;
    char *answer_buffer = b_serialize_to_memory(
        answer,
        answer_vtable->serialize,
        &size);
    *ex = b_zmq_send(
        socket_zmq,
        answer_buffer,
        size,
        flags);
    free(answer_buffer);
    if (*ex) {
        return;
    }
}

struct B_AnyAnswer *
b_protocol_recv_answer(
    void *socket_zmq,
    struct B_AnswerVTable const *answer_vtable,
    int flags,
    struct B_Exception **ex) {

    zmq_msg_t message;
    *ex = b_zmq_msg_init_recv(&message, socket_zmq, flags);
    if (*ex) {
        return NULL;
    }

    struct B_AnyAnswer *answer
        = b_deserialize_from_memory0(
            zmq_msg_data(&message),
            zmq_msg_size(&message),
            answer_vtable->deserialize);

    b_zmq_msg_close(&message);
    return answer;
}

void
b_protocol_send_identity_envelope(
    void *socket_zmq,
    struct B_Identity const *identity,
    int flags,
    struct B_Exception **ex) {

    *ex = b_zmq_send(
        socket_zmq,
        identity->data,
        identity->data_size,
        flags | ZMQ_SNDMORE);
    if (*ex) {
        return;
    }

    *ex = b_protocol_send_identity_delimiter(
        socket_zmq,
        flags);
    if (*ex) {
        return;
    }
}

struct B_Identity *
b_protocol_recv_identity_envelope(
    void *socket_zmq,
    int flags,
    struct B_Exception **ex) {

    zmq_msg_t identity_message;
    *ex = b_zmq_msg_init_recv(
        &identity_message,
        socket_zmq,
        flags);
    if (*ex) {
        return NULL;
    }

    *ex = b_protocol_recv_identity_delimiter(
        socket_zmq,
        flags);
    if (*ex) {
        b_zmq_msg_close(&identity_message);
        return NULL;
    }

    struct B_Identity *identity = b_identity_allocate(
        zmq_msg_data(&identity_message),
        zmq_msg_size(&identity_message));
    b_zmq_msg_close(&identity_message);
    return identity;
}

B_ERRFUNC
b_protocol_send_identity_delimiter(
    void *socket_zmq,
    int flags) {

    struct B_Exception *ex;

    ex = b_zmq_send(socket_zmq, NULL, 0, flags);
    if (ex) {
        return ex;
    }

    return NULL;
}

B_ERRFUNC
b_protocol_recv_identity_delimiter(
    void *socket_zmq,
    int flags) {

    struct B_Exception *ex;

    size_t delimiter_size = 0;
    ex = b_zmq_recv(
        socket_zmq,
        NULL,
        &delimiter_size,
        flags);
    if (ex) {
        return ex;
    }
    if (delimiter_size != 0) {
        return b_exception_string(
            "Expected empty delimiter");
    }

    return NULL;
}

void
b_protocol_send_worker_command(
    void *socket_zmq,
    enum B_WorkerCommand command,
    int flags,
    struct B_Exception **ex) {

    *ex = b_protocol_send_identity_delimiter(
        socket_zmq,
        flags | ZMQ_SNDMORE);
    if (*ex) {
        return;
    }

    uint8_t command_byte = command;
    *ex = b_zmq_send(
        socket_zmq,
        &command_byte,
        sizeof(command_byte),
        flags);
    if (*ex) {
        return;
    }
}

enum B_WorkerCommand
b_protocol_recv_worker_command(
    void *socket_zmq,
    int flags,
    struct B_Exception **ex) {

    uint8_t command_byte;
    size_t received_bytes = sizeof(command_byte);
    *ex = b_zmq_recv(
        socket_zmq,
        &command_byte,
        &received_bytes,
        flags);
    if (*ex) {
        return B_WORKER_READY;
    }
    if (received_bytes != sizeof(command_byte)) {
        *ex = b_exception_string(
            "Got too many bytes from worker");
        return B_WORKER_READY;
    }

    // TODO(strager): Validate byte.
    return (enum B_WorkerCommand) command_byte;
}

B_ERRFUNC
b_protocol_send_request_id(
    void *socket_zmq,
    struct B_RequestID const *request_id,
    int flags) {

    struct B_Exception *ex = b_zmq_send(
        socket_zmq,
        request_id->bytes,
        sizeof(request_id->bytes),
        flags);
    if (ex) {
        return ex;
    }

    return NULL;
}

B_ERRFUNC
b_protocol_recv_request_id(
    void *socket_zmq,
    struct B_RequestID *request_id,
    int flags) {

    size_t bytes_expected = sizeof(request_id->bytes);
    size_t bytes_received = bytes_expected;
    struct B_Exception *ex = b_zmq_recv(
        socket_zmq,
        request_id->bytes,
        &bytes_received,
        flags);
    if (ex) {
        return ex;
    }

    if (bytes_received != bytes_expected) {
        return b_exception_format_string(
            "Expected request ID (%zu bytes) but got %zu bytes",
            bytes_expected,
            bytes_received);
    }

    return NULL;
}
