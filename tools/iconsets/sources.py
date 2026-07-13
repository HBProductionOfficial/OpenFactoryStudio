"""Pinned upstream icon archives for build_iconsets.py. Every icon comes from these releases only."""
from __future__ import annotations

import hashlib
import shutil
import tarfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Source:
    key: str
    title: str
    version: str
    url: str
    sha256: str
    license: str
    homepage: str
    # Folder in the unpacked archive that holds the icons.
    root: str
    # License files (relative to the archive root) to copy next to the icons.
    license_files: tuple[str, ...]
    copyright: str


SOURCES: dict[str, Source] = {
    "nuvola": Source(
        key="nuvola",
        title="Nuvola icon theme",
        version="1.0",
        # icon-king.com is gone, so use the Internet Archive's copy of the original file.
        url="https://web.archive.org/web/20070114072052id_/http://www.icon-king.com/files/nuvola-1.0.tar.gz",
        sha256="7078c9a3d39951fe36f3e0e54494216737c10514b34e31ff13052c516f99b932",
        license="LGPL-2.1",
        homepage="https://en.wikipedia.org/wiki/Nuvola",
        root="nuvola",
        license_files=("nuvola/license.txt", "nuvola/readme.txt", "nuvola/author"),
        copyright="Copyright (c) 2003-2004 David Vignoni",
    ),
    "tabler": Source(
        key="tabler",
        title="Tabler Icons",
        version="3.48.0",
        url="https://registry.npmjs.org/@tabler/icons/-/icons-3.48.0.tgz",
        sha256="28447dcf6f0bb2b8d92c59a1b3d30900a180de2e97d7db4d267e6320dc68f449",
        license="MIT",
        homepage="https://tabler.io/icons",
        root="package/icons",
        license_files=("package/LICENSE",),
        copyright="Copyright (c) 2020-2026 Paweł Kuna",
    ),
    "lucide": Source(
        key="lucide",
        title="Lucide",
        version="1.51.0",
        url="https://registry.npmjs.org/lucide-static/-/lucide-static-1.51.0.tgz",
        sha256="6936026d1512256483161ae999982e3be3d5d74226996c4ec7447e901726a2d8",
        license="ISC",
        homepage="https://lucide.dev",
        root="package/icons",
        license_files=("package/LICENSE",),
        copyright="Copyright (c) 2026 Lucide Icons and Contributors",
    ),
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(source: Source, cache: Path) -> Path:
    """Returns the folder holding the unpacked archive, downloading it when needed."""
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / Path(source.url.rsplit("/", 1)[1])
    if not archive.is_file() or sha256_file(archive) != source.sha256:
        print(f"downloading {source.title} {source.version} ...")
        tmp = archive.with_suffix(archive.suffix + ".part")
        req = urllib.request.Request(source.url, headers={"User-Agent": "ofs-iconset-builder"})
        with urllib.request.urlopen(req, timeout=300) as resp, tmp.open("wb") as out:
            shutil.copyfileobj(resp, out)
        digest = sha256_file(tmp)
        if digest != source.sha256:
            tmp.unlink()
            raise RuntimeError(f"{source.url}: SHA-256 mismatch ({digest})")
        tmp.replace(archive)
    unpacked = cache / f"{source.key}-{source.version}"
    marker = unpacked / ".complete"
    if not marker.is_file():
        if unpacked.exists():
            shutil.rmtree(unpacked)
        unpacked.mkdir(parents=True)
        with tarfile.open(archive, "r:gz") as tar:
            tar.extractall(unpacked, filter="data")
        marker.write_text(source.sha256, encoding="utf-8")
    return unpacked
