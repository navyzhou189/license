
#ifndef LICS_INTERFACE_HH
#define LICS_INTERFACE_HH


/*
    NOTICES:
    algorithm id is very important, id depend by vendor.
*/
#define UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD  (100)
#define UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA  (101)
#define UNIS_VAS_OA (102)


enum AlgoLicsType {
	PICTURE = 0,
	VIDEO = 1,
};

typedef struct AlgoCapability_s {
    AlgoLicsType type;
    int algoID;
    int maxLimit;// only used for picture, which depends by different platforms, like NVIDIA TESLA T4/P4, CP14.
}AlgoCapability;

int lics_global_init(const char* remote, AlgoCapability* algoLics, int size);

int lics_apply(int algoID, const int expectLicsNum, int* actualLicsNum);

int lics_free(int algoID, const int licsNum);

void lics_global_cleanup();


const char* lics_version();


#endif