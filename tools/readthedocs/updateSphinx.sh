# create index.rst
python3 `pwd`/doc/sphinx_api/genrst.py `pwd`/escript esys `pwd`/tools/readthedocs/api
#copy over conf.py
cp `pwd`/doc/sphinx_api/conf.py `pwd`/tools/readthedocs/api/conf.py