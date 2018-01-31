struct param_struct {
    int iterations;
    int live_dangerously;
    int key;
    int permissions;
    int size;
};


void md5ify(char *, char *);
void say(const char *, char *);
int acquire_semaphore(const char *, int, int);
int release_semaphore(const char *, int, int);
void read_params(struct param_struct *);



