# Homebrew formula for xfig-qt.  It lives in the tap FabianHassler/homebrew-tap
# (Formula/xfig-qt.rb); this copy is kept with the sources.  For a release,
# set url to the tag's tarball and sha256 to `shasum -a 256` of it.
class XfigQt < Formula
  desc "Facility for Interactive Generation of figures (xfig) on Qt, without X11"
  homepage "https://github.com/FabianHassler/xfig-qt"
  url "https://github.com/FabianHassler/xfig-qt/archive/refs/tags/v3.2.9a-qt1.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "Xfig"
  head "https://github.com/FabianHassler/xfig-qt.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "pkgconf" => :build
  depends_on "fig2dev"
  depends_on "fontconfig"
  depends_on "freetype"
  depends_on "ghostscript"
  depends_on "jpeg-turbo"
  depends_on "libpng"
  depends_on "libtiff"
  depends_on :macos
  depends_on "qt"

  conflicts_with "xfig", because: "both install an `xfig` executable"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
    # the app bundle holds the program and xfig's data
    bin.write_exec_script prefix/"xfig.app/Contents/MacOS/xfig"
  end

  def caveats
    <<~EOS
      To start Xfig from the Finder or Launchpad, link it into Applications:
        ln -sf #{opt_prefix}/xfig.app /Applications/Xfig.app
    EOS
  end

  test do
    fig = prefix/"xfig.app/Contents/Resources/Libraries/Examples/pictures.fig"
    (testpath/"script.txt").write <<~EOS
      open #{fig}
      wait 300
      saveas #{testpath}/saved.fig
      quit
    EOS
    ENV["QT_QPA_PLATFORM"] = "offscreen"
    ENV["XFIGQT_SCRIPT"] = testpath/"script.txt"
    system bin/"xfig"
    assert_match "#FIG 3.2", (testpath/"saved.fig").read
  end
end
