{
  "targets": [
    {
      "target_name": "skybell",
      "sources": [ "module.cc", "getmedia.cc", "clientproxy.cc", "srtp.c" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "libraries":[
        "-lgcrypt"
      ]
    }
  ]
}
