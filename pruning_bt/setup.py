from setuptools import find_packages, setup
import os
from glob import glob
package_name = 'pruning_bt'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join("share", package_name, "launch"), glob("launch/*.launch.py")),
        (os.path.join("share", package_name, "config"), glob("config/*")),
        (os.path.join("share", package_name, "urdf"), glob("urdf/*")),
        (os.path.join("share", package_name, "srdf"), glob("srdf/*")),
        (os.path.join("share", package_name, "meshes/old"), glob("meshes/old/*")),

        (os.path.join("share", package_name, "meshes/"), glob("meshes/*.stl")),


        (os.path.join("share", package_name, "meshes/end_effectors/dovetail"), glob("meshes/end_effectors/dovetail/*")),

        (os.path.join("share", package_name, "meshes/end_effectors/mock_pruner"), glob("meshes/end_effectors/mock_pruner/*")),

        (os.path.join("share", package_name, "meshes/end_effectors/rotation_correction_plate"), glob("meshes/end_effectors/rotation_correction_plate/*")),

        (os.path.join("share", package_name, "meshes/base_mounts/farm-ng/amiga"), glob("meshes/base_mounts/farm-ng/amiga/*")),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='grimmlins',
    maintainer_email='abhinav98jain@gmail.com',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            
            'test_behavior_tree_node = pruning_bt.nodes.test_bt_node:main',
            'data_gathering_node = pruning_bt.nodes.data_gathering_node:main',
            'set_goal_service = pruning_bt.nodes.set_goal_service:main',
            'test_main = pruning_bt.test_main:main',
            'run_pruning_bt = pruning_bt.run_pruning_bt:main',
            'io_manager = pruning_bt.nodes.io_manager:main',
            'teleop_node = pruning_bt.nodes.teleop_node:main',
            'set_goal_as_endpoint_service = pruning_bt.nodes.set_goal_as_endpoint_service:main',
            'pub_pcl = pruning_bt.nodes.pub_pcl:main',
            'rviz_point_selection = pruning_bt.nodes.rviz_point_selection:main',
        ],
    },
)
