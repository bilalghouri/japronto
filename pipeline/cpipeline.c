#include <Python.h>
#include "structmember.h"

#include "cpipeline.h"
#include "capsule.h"


typedef struct Pipeline {
  PyObject_HEAD
  PyObject* ready;
  PyObject* task_done;
  PyObject* queue[10];
  size_t queue_start;
  size_t queue_end;
} Pipeline;


static PyObject*
Pipeline_new(PyTypeObject* type, PyObject* args, PyObject* kw)
{
  Pipeline* self = NULL;

  self = (Pipeline*)type->tp_alloc(type, 0);
  if(!self)
    goto finally;

  self->ready = NULL;
  self->task_done = NULL;

  finally:
  return (PyObject*)self;
}


static void
Pipeline_dealloc(Pipeline* self)
{
  Py_XDECREF(self->ready);
  Py_XDECREF(self->task_done);

  Py_TYPE(self)->tp_free((PyObject*)self);
}


static int
Pipeline_init(Pipeline* self, PyObject *args, PyObject* kw)
{
  int result = 0;

  if(!PyArg_ParseTuple(args, "O", &self->ready))
    goto error;

  Py_INCREF(self->ready);

  if(!(self->task_done = PyObject_GetAttrString((PyObject*)self, "_task_done")))
    goto error;

  self->queue_start = 0;
  self->queue_end = 0;

  goto finally;

  error:
  result = -1;

  finally:
  return result;
}

static PyTypeObject PipelineType;

static PyObject*
Pipeline_new_init(void)
{
  PyObject* self = Pipeline_new(&PipelineType, NULL, NULL);
  if(!self)
    goto error;

  if(Pipeline_init((Pipeline*)self, NULL, NULL) == -1)
    goto error;

  goto finally;

  error:
  self = NULL;

  finally:
  return self;
}


static PyObject*
Pipeline__task_done(Pipeline* self, PyObject* task)
{
  PyObject* result = Py_True;

  PyObject **queue_pos;
  for(queue_pos = self->queue + self->queue_start;
      queue_pos < self->queue + self->queue_end; queue_pos++) {
    PyObject* done = NULL;
    PyObject* done_result = NULL;

    if(!(done = PyObject_GetAttrString(*queue_pos, "done")))
      goto loop_error;

    if(!(done_result = PyObject_CallFunctionObjArgs(done, NULL)))
      goto loop_error;

    if(done_result == Py_False) {
      result = Py_False;
      goto loop_finally;
    }

    PyObject* tmp;
    if(!(tmp = PyObject_CallFunctionObjArgs(self->ready, *queue_pos, NULL)))
      goto loop_error;
    Py_DECREF(tmp);

    Py_DECREF(*queue_pos);

    goto loop_finally;

    loop_error:
    result = NULL;

    loop_finally:
    Py_XDECREF(done_result);
    Py_XDECREF(done);
    if(!result)
      goto error;
    if(result == Py_False)
      break;
  }

  self->queue_start = queue_pos - self->queue;

  goto finally;

  error:
  result = NULL;

  finally:
  Py_XINCREF(result);
  return result;
}


static PyObject*
Pipeline_queue(Pipeline* self, PyObject* task)
{
  PyObject* result = Py_None;
  PyObject* add_done_callback = NULL;

  if(self->queue_start == self->queue_end)
    self->queue_start = self->queue_end = 0;

  assert(self->queue_end < sizeof(self->queue) / sizeof(self->queue[0]));

  *(self->queue + self->queue_end) = task;
  Py_INCREF(task);

  self->queue_end++;

  if(!(add_done_callback = PyObject_GetAttrString(task, "add_done_callback")))
    goto error;

  PyObject* tmp;
  if(!(tmp = PyObject_CallFunctionObjArgs(add_done_callback, self->task_done, NULL)))
    goto error;
  Py_DECREF(tmp);

  goto finally;

  error:
  result = NULL;

  finally:
  Py_XDECREF(add_done_callback);
  Py_XINCREF(result);
  return result;
}


static PyObject*
Pipeline_get_empty(Pipeline* self, void* closure) {
  PyObject* result = self->queue_start == self->queue_end ? Py_True : Py_False;

  Py_INCREF(result);
  return result;
}

static PyMethodDef Pipeline_methods[] = {
  {"queue", (PyCFunction)Pipeline_queue, METH_O, ""},
  {"_task_done", (PyCFunction)Pipeline__task_done, METH_O, ""},
  {NULL}
};


static PyGetSetDef Pipeline_getset[] = {
  {"empty", (getter)Pipeline_get_empty, NULL, "", NULL},
  {NULL}
};


static PyTypeObject PipelineType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "cpipeline.Pipeline",       /* tp_name */
  sizeof(Pipeline),           /* tp_basicsize */
  0,                         /* tp_itemsize */
  (destructor)Pipeline_dealloc, /* tp_dealloc */
  0,                         /* tp_print */
  0,                         /* tp_getattr */
  0,                         /* tp_setattr */
  0,                         /* tp_reserved */
  0,                         /* tp_repr */
  0,                         /* tp_as_number */
  0,                         /* tp_as_sequence */
  0,                         /* tp_as_mapping */
  0,                         /* tp_hash  */
  0,                         /* tp_call */
  0,                         /* tp_str */
  0,                         /* tp_getattro */
  0,                         /* tp_setattro */
  0,                         /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,        /* tp_flags */
  "Pipeline",                /* tp_doc */
  0,                         /* tp_traverse */
  0,                         /* tp_clear */
  0,                         /* tp_richcompare */
  0,                         /* tp_weaklistoffset */
  0,                         /* tp_iter */
  0,                         /* tp_iternext */
  Pipeline_methods,          /* tp_methods */
  0,                         /* tp_members */
  Pipeline_getset,           /* tp_getset */
  0,                         /* tp_base */
  0,                         /* tp_dict */
  0,                         /* tp_descr_get */
  0,                         /* tp_descr_set */
  0,                         /* tp_dictoffset */
  (initproc)Pipeline_init,    /* tp_init */
  0,                         /* tp_alloc */
  Pipeline_new,              /* tp_new */
};


static PyModuleDef cpipeline = {
  PyModuleDef_HEAD_INIT,
  "cpipeline",
  "cpipeline",
  -1,
  NULL, NULL, NULL, NULL, NULL
};


PyMODINIT_FUNC
PyInit_cpipeline(void)
{
  PyObject* m = NULL;
  PyObject* api_capsule = NULL;

  if(PyType_Ready(&PipelineType) < 0)
    goto error;

  if(!(m = PyModule_Create(&cpipeline)))
    goto error;

  Py_INCREF(&PipelineType);
  PyModule_AddObject(m, "Pipeline", (PyObject*)&PipelineType);

  static Pipeline_CAPI capi = {
    Pipeline_new_init,
    Pipeline_queue
  };
  api_capsule = export_capi(m, "pipeline.cpipeline", &capi);
  if(!api_capsule)
    goto error;

  goto finally;

  error:
  m = NULL;

  finally:
  Py_XDECREF(api_capsule);
  return m;
}