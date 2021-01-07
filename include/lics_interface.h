
#ifndef LICS_INTERFACE_HH
#define LICS_INTERFACE_HH


/*
    NOTICES:
    algorithm id is very important, id depend by vendor.
*/
#define UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD  (100)
#define UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA  (101)
#define UNIS_VAS_OA (102)



int lics_init(int* algo_ids, int size);


int lics_apply(int algo_id, const int expect, int* actual);


int lics_cleanup();


const char* lics_version();


#endif