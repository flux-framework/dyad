#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to call Ollama model
static PyObject* run_ollama_model(PyObject* self, PyObject* args) {
    const char *prompt;
    const char *model = "phi3";
    char command[1024];
    char response[4096] = {0};
    FILE *fp;

    // Parse arguments
    if (!PyArg_ParseTuple(args, "s|s", &prompt, &model)) {
        return NULL;
    }

    // Construct command
    snprintf(command, sizeof(command), "ollama run %s '%s'", model, prompt);

    // Execute command
    fp = popen(command, "r");
    if (fp == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to run Ollama model");
        return NULL;
    }

    // Read response
    fread(response, sizeof(char), sizeof(response) - 1, fp);
    pclose(fp);

    // Return response
    return PyUnicode_FromString(response);
}

// Method definitions
static PyMethodDef DyadOllamaMethods[] = {
    {"run_ollama_model", run_ollama_model, METH_VARARGS, "Run an Ollama model"},
    {NULL, NULL, 0, NULL}  // Sentinel
};

// Module definition
static struct PyModuleDef dyadollamamodule = {
    PyModuleDef_HEAD_INIT,
    "dyad_ollama",  // Module name
    NULL,           // Module documentation
    -1,             // Size of per-interpreter state
    DyadOllamaMethods
};

// Module initialization
PyMODINIT_FUNC PyInit_dyad_ollama(void) {
    return PyModule_Create(&dyadollamamodule);
}