/*
 * Contains rough code for executor.c and tests functions written in executor.c
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <ffi.h>

int main(int argc, char* argv) {

	void *lib_handle;
	double (*pow)(double, double);
	const char *dlsym_error;

	char *library_file = "/usr/lib/libm.dylib";
	char *routine_name = "pow";
	void (*routine_handle);

	// load the library libm
	lib_handle = dlopen(library_file, RTLD_LAZY);

	// check we could open the library
	if(!lib_handle) {
		printf("Cannot open library: %s\n", dlerror());
		return 1;
	} else {
		printf("Library opened: %s\n", library_file);
	}

		//reset errors
	dlerror();

	routine_handle = dlsym(lib_handle, routine_name);

	dlsym_error = dlerror();

	if(dlsym_error) {
		printf("Cannot load symbol: 'pow': %s\n", dlsym_error);
		dlclose(lib_handle);
		return 1;
	} else {
		printf("Symbol loaded: %s\n", routine_name);
	}


	//printf("foo: %.2f\n", (double) routine_handle(4.0, 3.0));
	{
		ffi_cif cif;
		ffi_type **args;
		int n_args;
		void **values;
		double *rc;
		int i;
		double a, b;

		args = malloc(n_args*sizeof(ffi_type*));
		values = malloc(n_args*sizeof(void*));
		rc = malloc(sizeof(double));

		if(!args || !values || !rc) {
			printf("Could not allocate memory for ffi\n");
			return 1;
		} else {
			printf("ffi memory allocated\n");
		}

		for(i=0; i<n_args; i++) {
			args[i] = &ffi_type_double;

		}

		values[0] = &a;
		values[1] = &b;

		if(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, n_args, &ffi_type_double, args) == FFI_OK) {

		   	ffi_call(&cif, routine_name, rc, values);

		   	printf("%d\n", (double)*rc);
		}


		for(i=0;i<n_args;i++) {
			free(values[i]);
		}
	}
	dlclose(lib_handle);
	return 0;
}


