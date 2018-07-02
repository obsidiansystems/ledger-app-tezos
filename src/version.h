#ifdef BAKING_APP
#define CLASS 1
#else
#define CLASS 0
#endif

#define MAJOR 1
#define MINOR 0
#define PATCH 0

typedef struct version {
    uint8_t class;
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} version_t;

const version_t version = { CLASS, MAJOR, MINOR, PATCH };
