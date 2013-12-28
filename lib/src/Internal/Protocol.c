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
    struct B_Broker const *broker) {

    int rc = snprintf(
        buffer,
        buffer_size,
        "inproc://b_client_%p",
        broker);
    return rc >= 0 && rc == strlen(buffer);
}

bool
b_protocol_worker_endpoint(
    char *buffer,
    size_t buffer_size,
    struct B_Broker const *broker) {

    int rc = snprintf(
        buffer,
        buffer_size,
        "inproc://b_worker_%p",
        broker);
    return rc >= 0 && rc == strlen(buffer);
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

    *ex = b_zmq_send(
        socket_zmq,
        NULL,
        0,
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

    size_t delimiter_size = 0;
    *ex = b_zmq_recv(
        socket_zmq,
        NULL,
        &delimiter_size,
        flags);
    if (*ex) {
        b_zmq_msg_close(&identity_message);
        return NULL;
    }
    if (delimiter_size != 0) {
        *ex = b_exception_string("Expected empty delimiter");
        b_zmq_msg_close(&identity_message);
        return NULL;
    }

    struct B_Identity *identity = b_identity_allocate(
        zmq_msg_data(&identity_message),
        zmq_msg_size(&identity_message));
    b_zmq_msg_close(&identity_message);
    return identity;
}

void
b_protocol_send_worker_command(
    void *socket_zmq,
    enum B_WorkerCommand command,
    int flags,
    struct B_Exception **ex) {

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