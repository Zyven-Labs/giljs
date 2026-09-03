{
  "targets": [
    {
      "target_name": "gil_napi",
      "sources": ["src/gil_napi.cc"],
      "include_dirs": [
        "node_modules/node-addon-api",
        "libgil/include"
      ],
      "libraries": [
        "../libgil/libgil.a"
      ],
      "defines": [ "NAPI_CPP_EXCEPTIONS" ],
      "cflags_cc": [ "-std=c++17", "-fexceptions" ]
    }
  ]
}
