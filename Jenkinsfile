

def rocmnode(name) {
    def node_name = 'rocmtest'
    if(name == 'fiji') {
        node_name = 'rocmtest && fiji';
    } else if(name == 'vega') {
        node_name = 'rocmtest && vega';
    } else if(name == 'vega10') {
        node_name = 'rocmtest && vega10';
    } else if(name == 'vega20') {
        node_name = 'rocmtest && vega20';
    } else if(name == 'gfx908') {
        node_name = 'gfx908';
    } else {
        node_name = name
    }
    return node_name
}



def cmake_build(compiler, flags, env4make, prefixpath){
    def workspace_dir = pwd()
    def vcache = "/var/jenkins/.cache/miopen/vcache"
    def archive = (flags == '-DCMAKE_BUILD_TYPE=release')
    def config_targets = "check doc MIOpenDriver"
    def test_flags = "--disable-verification-cache"
    def debug_flags = "-g -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=undefined"
    def compilerpath = ""
    def configargs = ""
    if (prefixpath == "")
        compilerpath = compiler;
    else
    {
        compilerpath = prefixpath + "/bin/" + compiler
        configargs = "-DCMAKE_PREFIX_PATH=${prefixpath}"
    }

    if (archive == true) {
        config_targets = "package"
    }
    def cmd = """
        echo \$HSA_ENABLE_SDMA
        ulimit -c unlimited
        rm -rf build
        mkdir build
        cd build
        CXX=${compilerpath} CXXFLAGS='-Werror' cmake ${configargs} -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS='${test_flags}' -DCMAKE_CXX_FLAGS_DEBUG='${debug_flags}' ${flags} .. 
        MIOPEN_DEBUG_CONV_IMPLICIT_GEMM_XDLOPS=1 CTEST_PARALLEL_LEVEL=4 MIOPEN_VERIFY_CACHE_PATH=${vcache} MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 ${env4make} dumb-init make -j\$(nproc) ${config_targets}
    """
    echo cmd
    sh cmd
    // Only archive from master or develop
    if (archive == true && (env.BRANCH_NAME == "develop" || env.BRANCH_NAME == "master")) {
        archiveArtifacts artifacts: "build/*.deb", allowEmptyArchive: true, fingerprint: true
    }
}

def buildJob(compiler, flags, env4make, image, prefixpath="/opt/rocm", cmd = ""){

        env.HSA_ENABLE_SDMA=0 
        checkout scm
        def dockerOpts="--device=/dev/kfd --device=/dev/dri --group-add video --cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
        def dockerArgs = "--build-arg PREFIX=${prefixpath} "
        if(prefixpath == "")
        {
            dockerArgs = ""
        }
        def retimage
        try {
            retimage = docker.build("${image}", dockerArgs + '.')
            withDockerContainer(image: image, args: dockerOpts) {
                timeout(time: 5, unit: 'MINUTES')
                {
                    sh 'PATH="/opt/rocm/opencl/bin/x86_64/:$PATH" clinfo'
                }
            }
        } catch(Exception ex) {
            retimage = docker.build("${image}", dockerArgs + "--no-cache .")
            withDockerContainer(image: image, args: dockerOpts) {
                timeout(time: 5, unit: 'MINUTES')
                {
                    sh 'PATH="/opt/rocm/opencl/bin/x86_64/:$PATH" clinfo'
                }
            }
        }

        withDockerContainer(image: image, args: dockerOpts + ' -v=/var/jenkins/:/var/jenkins') {
            timeout(time: 5, unit: 'HOURS')
            {
                if(cmd == ""){
                    cmake_build(compiler, flags, env4make, prefixpath)
                }else{
                    sh cmd
                }
            }
        }
        return retimage
}

def buildHipClangJob(compiler, flags, env4make, image, prefixpath="/opt/rocm", cmd = ""){

        env.HSA_ENABLE_SDMA=0 
        checkout scm
        def dockerOpts="--device=/dev/kfd --device=/dev/dri --group-add video --cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
        def dockerArgs = "--build-arg PREFIX=${prefixpath} -f hip-clang.docker "
        def retimage
        try {
            retimage = docker.build("${image}", dockerArgs + '.')
            withDockerContainer(image: image, args: dockerOpts) {
                timeout(time: 5, unit: 'MINUTES')
                {
                    sh 'PATH="/opt/rocm/opencl/bin:/opt/rocm/opencl/bin/x86_64:$PATH" clinfo'
                }
            }
        } catch(Exception ex) {
            retimage = docker.build("${image}", dockerArgs + "--no-cache .")
            withDockerContainer(image: image, args: dockerOpts) {
                timeout(time: 5, unit: 'MINUTES')
                {
                    sh 'PATH="/opt/rocm/opencl/bin:/opt/rocm/opencl/bin/x86_64:$PATH" clinfo'
                }
            }
        }

        withDockerContainer(image: image, args: dockerOpts + ' -v=/var/jenkins/:/var/jenkins') {
            timeout(time: 5, unit: 'HOURS')
            {
                if(cmd == ""){
                    cmake_build(compiler, flags, env4make, prefixpath)
                }else{
                    sh cmd
                }
            }
        }
        return retimage
}



pipeline {
    agent none 
    options {
        parallelsAlwaysFailFast()
    }
    environment{
        image = "miopen"
    }
    stages{
        // Run all static analysis tests

        stage("Kamil's custom stages Forward"){
            parallel{
                stage('Forward clang') {
                    agent{ label rocmnode("gfx908") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug .. 
                            make -j\$(nproc)

                            export MIOPEN_FIND_MODE=NORMAL;
                            export MIOPEN_DEBUG_CONV_IMPLICIT_GEMM_XDLOPS=1 ;
                            export MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0;
                            export MIOPEN_LOG_LEVEL=3;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=64;
                            ./bin/MIOpenDriver   conv -x 1 -y 1 -W 25 -H 25 -c 1600 -n 32 -k 1600 -g 25 -p 0 -q 0 -time 1 -F 1 -V 1;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=72;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;                            
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=64;
                            ./bin/MIOpenDriver   conv -x 1 -y 1 -W 19 -H 19 -c 2304 -n 32 -k 2304 -g 36 -p 0 -q 0 -time 1 -F 1 -V 1;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=73;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=64;
                            ./bin/MIOpenDriver   conv -x 1 -y 1 -W 15 -H 15 -c 3136 -n 32 -k 3136 -g 49 -p 0 -q 0 -time 1 -F 1 -V 1;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=74;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;


                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 128 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 324 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10  -H 10 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19  -H 19 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;

                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 128 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486   -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 99 -n 13 -k 99 -g 11 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 13 -n 7 -k 13 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;

                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=73
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 128 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 324 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10  -H 10 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19  -H 19 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 128 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486   -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 99 -n 13 -k 99 -g 11 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 13 -n 7 -k 13 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=74
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 128 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 324 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10  -H 10 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19  -H 19 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;

                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 128 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486   -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 99 -n 13 -k 99 -g 11 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 13 -n 7 -k 13 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;

                            export MIOPEN_DEBUG_FIND_ONLY_SOLVER=75
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 128 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 128 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 256 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38  -H 38 -c 256 -n 32 -k 324 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10  -H 10 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19  -H 19 -c 512 -n 32 -k 486 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver   conv -x 3 -y 3 -W 75 -H 75 -c 64 -n 32 -k 64 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 128 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 128 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 5 -H 5 -c 256 -n 32 -k 486   -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 256 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 4 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 99 -n 13 -k 99 -g 11 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 9 -H 9 -c 13 -n 7 -k 13 -g 1 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 38 -H 38 -c 256 -n 32 -k 324 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 10 -H 10 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;
                            ./bin/MIOpenDriver conv -x 3 -y 3 -W 19 -H 19 -c 512 -n 32 -k 486 -g 32 -p 1 -q 1 -time 1 -F 3 -V 1;

                        """

                    }
                    steps{
                        buildHipClangJob('/opt/rocm/llvm/bin/clang++', '', "", image+'-hip-clang', "/usr/local", cmd)
                    }
                }
            }
        }

        //stage("Kamil's custom stages Backwards"){
        //    parallel{
        //        stage('Backwards clang') {
        //            agent{ label rocmnode("gfx908") }
        //            environment{
        //                cmd = """
        //                    ulimit -c unlimited
        //                    rm -rf build
        //                    mkdir build
        //                    cd build
        //                    CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug .. 
        //                    make -j\$(nproc) 
        //                    MIOPEN_DEBUG_AMD_WINOGRAD_3X3=0 MIOPEN_LOG_LEVEL=6 ./bin/MIOpenDriver conv -x 3 -y 3 -W 28 -H 28 -c 192 -n 16 -k 32 -g 1 -p 1 -q 1 -time 1 -F 2 -V 1
        //                """
        //
        //            }
        //            steps{
        //                buildHipClangJob('/opt/rocm/llvm/bin/clang++', '', "", image+'-hip-clang', "/usr/local", cmd)
        //            }
        //        }
        //        stage('Backwards hcc') {
        //            agent{ label rocmnode("gfx908") }
        //            environment{
        //                cmd = """
        //                    ulimit -c unlimited
        //                    rm -rf build
        //                    mkdir build
        //                    cd build
        //                    CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug .. 
        //                    make -j\$(nproc) 
        //                    MIOPEN_DEBUG_AMD_WINOGRAD_3X3=0 MIOPEN_LOG_LEVEL=6 ./bin/MIOpenDriver conv -x 3 -y 3 -W 28 -H 28 -c 192 -n 16 -k 32 -g 1 -p 1 -q 1 -time 1 -F 2 -V 1
        //                """
        //
        //            }
        //            steps{
        //                buildJob('hcc', '', "", image + "rocm", "/usr/local", cmd)
        //            }
        //        }
        //    }
        //}
    }    
}

