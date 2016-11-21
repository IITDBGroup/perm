#!/bin/bash

# EXIT if not linux build on master
if [ "$TRAVIS_BRANCH" != "master" ]
then
  echo "This commit was made against the $TRAVIS_BRANCH and not the master! No deploy!"
  exit 0
fi
if [[ $TRAVIS_OS_NAME == 'osx' ]]
then
  echo "This build is not our default deployment build! No deploy!"
  exit 0
fi


# deploy:
  # provider: releases
  # api_key:
  #   secure: Ba3gjgPKEOlrB5IUeqUgzaohCWfTAU3Xo8GLGjcY4wCk7OWtM/upzyMECXASmCUt5nn7ZIHE+r+FpgPG2mZwwNzKaUJ1GNTB8vdewkyhcfCd8ZPa7tJWDbq/sSh9Wfl8Cr3bCokD1V7dNSbBEt2U0vffeL/+hQq7DWyoLJ1FkEnHd/iJvpuxqahahkqxK+geA7Cjq8Nevgh33DSxZbBqRwQot+WE4BzkFGnq09vRjvO1RLSPYn9RvDSYH3s2QwZixofXsJtEIM/iN8U5w9hC5FvDairlomHkyvZDiBrL5l2COJS212rbT0gmef6CcnS7B/pRCilm1t31icwG/MNkve2JkSLMjvU1kOaijwjnrrb2FZ8K4+25l1pnRlbsYcR1/eDtlfFwWaP2DvIyc1NQse20PET0R7h3mSRyEr6o1PrrVHa8yssNZmXQ51quiLg7lm0PJX6FL1Azf3NhvQ4p60l53lxf3lgB79YCR2VYx7/y0LWfyDED50YCnadzsYniZiZpFQzc0uh8+//0dzvJ8xuEM4G0B9cyxzJ8yA9zyQFDAJAJQy4V9Z/tifGUrPaGeHLwIrz7NQ19iIAzd7qAI+5utamCo6pvL+y3jr//l/LzujWohKgbj+Rt7megLNkWTYdDDzevQU30M2tt2uW8vfp9BxzieinBZ5iKcNVMZs8=
  # file: perm-0.1-src.tar.gz
  # on:
  #   repo: IITDBGroup/perm


