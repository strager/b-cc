CFLAGS += -g -Wall

.PHONY: JoinFiles
JoinFiles:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $(@) \
		-I Headers \
		-I PrivateHeaders \
		-I Vendor/sqlite-3.8.4.1 \
		Vendor/sqlite-3.8.4.1/sqlite3.c \
		Examples/JoinFiles/Source/Main.c \
		Source/AnswerContext.c \
		Source/AnswerFuture.c \
		Source/Assertions.c \
		Source/Database.c \
		Source/FileQuestion.c \
		Source/Main.c \
		Source/Memory.c \
		Source/Mutex.c \
		Source/QuestionAnswer.c \
		Source/SQLite3.c \
		Source/Serialize.c \
		Source/UUID.c
