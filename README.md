# ZMQInterface plugin
This reposiory contains the following first party plugin developed by Open Ephys that make use of the [0MQ library](http://zeromq.org/):

**ZMQInterface**: A plugin enabling the interfacing of ZeroMQ clients to Open Ephys. The interface exposes all data and events and allows to provide events to the application, enabling the creation of advanced visualization and monitoring add-ons. For more information, go to our [wiki](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/1547206701/ZMQInterface) and find out how to use this plugin. 

## Installation
### Installing the 0MQ library
For windows and linux, the required files are already included for the plugin
We have detailed instructions on our wiki on how to install the 0MQ for [macOS](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/491555/macOS).

### Building the plugins
Building the plugins requires [CMake](https://cmake.org/). Detailed instructions on how to build open ephys plugins with CMake can be found in [our wiki](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/1259110401/Plugin+CMake+Builds).
We highly recommend building all three projects simultaneously using the configuration provided in the top-level Build folder, instead of the individual plugins. Building the plugins individually will need manual tweaking for them to find the OpenEphysHDF5 common library.

## Attribution
Originally developed by [Francesco Battaglia](https://github.com/fpbattaglia) at [Memory Dynamics Lab](https://www.memorydynamics.org/).
Updated and maintained by [András Széll](https://github.com/aszell).