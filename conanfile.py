from conans import ConanFile, CMake, tools


class NetConan(ConanFile):
    name = "wildcat-ws"
    version = "0.2.0"
    license = "MIT"
    author = "<Ross Bennett> <rossbennett34@gmail.com>"
    url = "https://github.com/rossb34/wildcat-ws"
    description = "Web socket library"
    exports_sources = "include/*"
    no_copy_source = True

    def package(self):
        self.copy("*.hpp")

    def package_id(self):
        self.info.header_only()
