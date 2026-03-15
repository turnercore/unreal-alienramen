import logging
import os.path
import shutil
import subprocess

from mkdocs.config import config_options as mkd
from mkdocs.plugins import BasePlugin

from .configitems import ConfigItems

logger = logging.getLogger("mkdocs")


def get_doxygen_key(doxyfile, key, default=None):
    try:
        return next(
            line.split("=")[1].strip()
            for line in open(doxyfile)
            if line.strip().startswith(key)
        )
    except StopIteration:
        if default is not None:
            return default
        raise RuntimeError(
            "Could not find a value for '{0}' in doxygen config file '{1}'".format(
                key, doxyfile
            )
        )


def runDoxygen(basedir, cfg=None, workdir=None, dest=None, tryClone=False, recursive=False):
    if os.path.isdir(basedir):
        basedir = os.path.abspath(basedir)

        if cfg is None:
            default_cfgs = [os.path.join(basedir, cfg_name) for cfg_name in ("Doxyfile", "doxygen.cfg")]
            for cfg_try in default_cfgs:
                if os.path.exists(cfg_try):
                    cfg = cfg_try
                    break
            if cfg is None:
                raise RuntimeError(
                    "No doxygen with default name found in {0}. please specify the name through the 'config' key".format(
                        basedir
                    )
                )
        else:
            if not os.path.isabs(cfg):
                cfg = os.path.join(basedir, cfg)
            if not os.path.exists(cfg):
                raise ValueError("File {0} not found".format(cfg))

        if workdir is None:
            workdir = os.path.dirname(cfg)
        else:
            if not os.path.isabs(workdir):
                workdir = os.path.join(os.path.dirname(cfg), workdir)
            if not os.path.isdir(workdir):
                raise ValueError(
                    "Working directory {0} for running doxygen not found".format(workdir)
                )

        outpath = os.path.join(
            os.path.join(workdir, get_doxygen_key(cfg, "OUTPUT_DIRECTORY", ".")),
            get_doxygen_key(cfg, "HTML_OUTPUT", default="html"),
        )
        doxylog = "doxygen.log"
        with open(doxylog, "w") as logfile:
            subprocess.check_call(["doxygen", cfg], cwd=workdir, stdout=logfile, stderr=logfile)
        shutil.move(outpath, dest)
        shutil.move(doxylog, dest)
    else:
        if not tryClone:
            raise ValueError(
                "No such directory: {0} (set 'tryclone' if you want to clone from a remote url)".format(
                    basedir
                )
            )

        from urllib.parse import urlparse

        parsed = urlparse(basedir)
        if not (parsed.scheme and parsed.netloc):
            raise ValueError(
                "'{0}' represents neither an existing directory nor a valid URL".format(
                    basedir
                )
            )

        from tempfile import TemporaryDirectory

        with TemporaryDirectory() as tmp_dir:
            repo_name = os.path.split(basedir)[-1].split(".")[0] or "repo"
            repo_path = os.path.join(tmp_dir, repo_name)
            clone_args = ["git", "clone", "--depth", "1"]
            if recursive:
                clone_args.insert(2, "--recursive")
            clone_args.extend([basedir, repo_path])
            subprocess.check_call(clone_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            runDoxygen(
                repo_path,
                cfg=cfg,
                workdir=workdir,
                dest=dest,
                tryClone=False,
                recursive=False,
            )


class DoxygenPlugin(BasePlugin):
    config_scheme = (
        (
            "packages",
            ConfigItems(
                ("url", mkd.Type(str)),
                ("config", mkd.Type(str)),
                ("workdir", mkd.Type(str)),
            ),
        ),
        ("tryclone", mkd.Type(bool, default=False)),
        ("recursive", mkd.Type(bool, default=False)),
    )

    def on_post_build(self, config):
        for package_config in self.config["packages"]:
            for outname, cfg in package_config.items():
                outpath = os.path.abspath(os.path.join(config["site_dir"], outname))
                try:
                    basedir = cfg.get("url", ".")
                    inner_cfg = cfg.get("config")
                    logger.info(
                        "Running doxygen for {0} with {1}, saving into {2}".format(
                            basedir if basedir != "." else "current directory",
                            inner_cfg if inner_cfg else "default config",
                            outpath,
                        )
                    )
                    runDoxygen(
                        basedir,
                        cfg=inner_cfg,
                        workdir=cfg.get("workdir"),
                        dest=outpath,
                        tryClone=self.config["tryclone"],
                        recursive=self.config["recursive"],
                    )
                except Exception as exc:
                    logger.error("Skipped doxygen for package {0}: {1!s}".format(outname, exc))
