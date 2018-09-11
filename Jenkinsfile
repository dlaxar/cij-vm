pipeline {
    agent any

    stages {
        stage ('Checkout') {
            steps {
                checkout scm
                sh 'git submodule update --init'
            }
        }

        stage ('Build') {
            steps {
                sh 'cmake -DCMAKE_BUILD_TYPE=Debug .'
                sh 'make vm'
            }
        }

        stage ('Test') {
            steps {
                sh 'make tests && ./tests --success --reporter compact'
            }
        }
    }
}
