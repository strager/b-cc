#ifndef UUID_H_5D99EDF5_A544_42C2_9442_1F8348898FC1
#define UUID_H_5D99EDF5_A544_42C2_9442_1F8348898FC1

// A stable, unique identifier.  Used to identify different
// types stably across builds and for persistence.  Stored
// as a 0-terminated ASCII string.
//
// A UUID may be generated using the 'uuidgen' command-line
// utility on Linux or Mac OS X:
//
// $ uuidgen
// 93C02C48-790A-415D-858E-D98C93941FEB
// $ uuidgen
// 5CCEEAF6-4B90-4124-8328-71B3A3279779
struct B_UUID {
    const char *uuid;
};

// Converts a string literal into a UUID.
#define B_UUID(str) ((struct B_UUID) { .uuid = "" str "" })

// Converts a string which will not be deallocated into a
// UUID.
UUID
b_uuid_from_stable_string(const char *);

// Converts a string which may be deallocated into a UUID.
// Leaks memory by keeping copies of strings for the
// lifetime of the application.
UUID
b_uuid_from_temp_string(const char *);

// Compares two UUIDs using string comparison.  Returns
// 'true' if the two UUIDs are equivalent.
bool
b_uuid_equal(struct B_UUID, struct B_UUID);

#endif
