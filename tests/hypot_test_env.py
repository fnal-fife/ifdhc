
import os
import sys

def hypot_test_env():
    
    with os.popen("""   
        . /cvmfs/fermilab.opensciencegrid.org/packages/common/setup-env.sh
        spack load --sh --first  data-dispatcher@1.27.0 os=default_os metacat@4.0.2 os=default_os
    """, "r") as pf:
        for line in pf:
            if line.find("=") > 0 and line.find("export") == 0:
                var, val = line.split("=",1)
                var = var.replace("export ","").strip()
                os.environ[var] = val

    # setup local ifdh:
    os.environ["PATH"] = f"{os.getcwd()}/../bin:{os.environ['PATH']}"
    os.environ["PYTHONPATH"] = f"{os.getcwd()}/../lib/python:{os.environ.get('PYTHONPATH','')}"
    sys.path.insert(1,f"{os.getcwd()}/../lib/python")
    os.environ["IFDHC_CONFIG_DIR"] = f"{os.getcwd()}/.."

    os.environ["IFDH_PROXY_ENABLE"] = "0"

    os.environ["GROUP"] = "hypot"
    os.environ["EXPERIMENT"] = "hypot"
    os.environ["SAM_WEB_BASE_URL"] = "https://samdev.fnal.gov:8483/sam/samdev/api"
    os.environ["IFDH_BASE_URI"] = "https://samdev.fnal.gov:8483/sam/samdev/api"
    os.environ["METACAT_SERVER_URL"] = "https://metacat.fnal.gov:9443/hypot_meta_dev/app"
    os.environ["DATA_DISPATCHER_URL"] = "https://metacat.fnal.gov:9443/hypot_dd/data"
    os.environ["DATA_DISPATCHER_AUTH_URL"] = "https://metacat.fnal.gov:8143/auth/hypot_dev"
    os.environ["METACAT_AUTH_SERVER_URL"] = "https://metacat.fnal.gov:8143/auth/hypot_dev"

    import ifdh
    h = ifdh.ifdh()
    print(f"ifdh getToken(): {h.getToken()})")


