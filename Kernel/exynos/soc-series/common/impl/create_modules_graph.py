import sys
import argparse
import logging
import pathlib
import textwrap
from typing import Sequence, TextIO
from collections import deque

BUILTIN_DRV="built-in-driver"
ROOT = "ROOT"

class KernelModulesGraph:
    def __init__(self, modules_dep):

        self.modules_dep_contents = modules_dep.readlines()
        self.reordered_module_list = []
        self.module_number=len(self.modules_dep_contents)
        self.ModPath2Index= dict()
        self.Index2ModPath = dict()
        self.ModName2ModPath = dict()
        self.ModPath2ModName = dict()
        self.first_stage_module_list = []
        self.insmod_module_list = []

        self.ModPath2Index[ROOT] = 0
        self.Index2ModPath[0] = ROOT
        self.ModName2ModPath[ROOT] = ROOT
        self.ModPath2ModName[ROOT] = ROOT

        index = 1
        for module_path in self.modules_dep_contents:
            module_path = module_path.strip().split(':')[0]
            mod_name = module_path.split('/')[-1].rsplit('.', 1)[0]
            self.ModPath2Index[module_path] = index #ModPath2Index[path/module_name] = 0 or 1 ...
            self.Index2ModPath[index] = module_path #Index2ModPath[0 or 1 ...] = path/module_name
            self.ModName2ModPath[mod_name] = module_path #ModName2ModPath[module_name] = path/module_name
            self.ModPath2ModName[module_path] = mod_name #ModPath2ModName[path/module_name] = module_name
            index+=1

        self.module_graph = [[] for i in range(self.module_number + 1)]
        self.reverse_module_graph = [[] for i in range(self.module_number + 1)]
        self.indegree = [0] * (self.module_number+ 1)

    def create_module_graph(self):

        for module_dep in self.modules_dep_contents:
            module_dep = module_dep.strip()

            vortex,e_str = module_dep.split(':')
            edges_list= e_str.split()

            if not edges_list:
                self.module_graph[self.ModPath2Index[ROOT]].append(self.ModPath2Index[vortex])
                self.reverse_module_graph[self.ModPath2Index[vortex]].append(self.ModPath2Index[ROOT])
                self.indegree[self.ModPath2Index[vortex]] += 1
                cycle = self.__find_cycle_start_node()
                if cycle is True:
                    print("\033[33mWARNING: \033[0m" + "Module cycle was detected. (" + self.ModPath2ModName[ROOT] + " --> " + self.ModPath2ModName[vortex] + ")")
                    self.module_graph[self.ModPath2Index[ROOT]].remove(self.ModPath2Index[vortex])
                    self.indegree[self.ModPath2Index[vortex]] -= 1
                    self.reverse_module_graph[self.ModPath2Index[vortex]].remove(self.ModPath2Index[ROOT])

            for edge in edges_list:
                edge = edge.strip(' ')
                self.module_graph[self.ModPath2Index[edge]].append(self.ModPath2Index[vortex])
                self.reverse_module_graph[self.ModPath2Index[vortex]].append(self.ModPath2Index[edge])
                self.indegree[self.ModPath2Index[vortex]] += 1
                cycle = self.__find_cycle_start_node()
                if cycle is True:
                    print("\033[33mWARNING: \033[0m" + "Module cycle was detected. (" + self.ModPath2ModName[edge] + " --> " + self.ModPath2ModName[vortex] + ")")
                    self.module_graph[self.ModPath2Index[edge]].remove(self.ModPath2Index[vortex])
                    self.indegree[self.ModPath2Index[vortex]] -= 1
                    self.reverse_module_graph[self.ModPath2Index[vortex]].remove(self.ModPath2Index[edge])

        self.__update_fw_devlink()

        return 0

    def insert_fw_devlink(self, fw_devlink, modules_alias):
        self.fw_devlink_list = []
        try:
            self.fw_devlink_contents = fw_devlink.readlines()
            self.modules_alias_contents = modules_alias.readlines()
        except AttributeError:
            print("\033[33mWARNING: \033[0m" + "Failed to find fw_devlink file")
            return -1

        for line in self.fw_devlink_contents:
            line = line.strip()
            supplier, consumer = line.split(' --> ')

            supplier = supplier.split('(null)')[1] if "(null)" in supplier else supplier
            consumer = consumer.split('(null)')[1] if "(null)" in consumer else consumer

            sup_mod = ""
            for alias_line in self.modules_alias_contents:
                alias_line =alias_line.strip('alias ').strip()
                if (supplier + " ") in alias_line:
                    sup_mod = alias_line.split(' ')[1]

            if not sup_mod:
                sup_mod = BUILTIN_DRV

            con_mod = ""
            for alias_line in self.modules_alias_contents:
                alias_line = alias_line.strip('alias ').strip()
                if (consumer + " ") in alias_line:
                    con_mod = alias_line.split(' ')[1]

            if not con_mod:
                con_mod = BUILTIN_DRV

            if sup_mod not in BUILTIN_DRV and con_mod not in BUILTIN_DRV:
                self.fw_devlink_list.append((sup_mod, con_mod))

        return 0

    def __find_cycle_start_node(self):
        def __dfs(node, visited):
            visited[node] = "visiting"

            for neighbor in self.module_graph[node]:
                if visited[neighbor] == "visiting":
                    return True
                elif visited[neighbor] == "unvisited" and __dfs(neighbor, visited):
                    return True

            visited[node] = "visited"
            return False

        num_nodes = len(self.module_graph)
        visited = ["unvisited"] * num_nodes
        if __dfs(0, visited):
            return True

        return False


    def __update_fw_devlink(self):
        if self.fw_devlink_list is None:
            print("\033[33mWARNING: \033[0m" + "There is nothing in the fw_devlink list")
            print("\033[33mWARNING: \033[0m" + "Skip fw_devlink")
            return 0

        for fw_devlink in self.fw_devlink_list:

            supplier = fw_devlink[0]
            consumer = fw_devlink[1]

            for name in self.ModName2ModPath.keys():
                replace_name = name.replace("-", "_")
                if supplier == replace_name:
                    supplier = name
                    break

            for name in self.ModName2ModPath.keys():
                replace_name = name.replace("-", "_")
                if consumer == replace_name:
                    consumer = name
                    break

            parent = self.ModName2ModPath[supplier]
            child = self.ModName2ModPath[consumer]

            if self.ModPath2Index[child] not in self.module_graph[self.ModPath2Index[parent]]:
                self.module_graph[self.ModPath2Index[parent]].append(self.ModPath2Index[child])
                self.indegree[self.ModPath2Index[child]] += 1
                self.reverse_module_graph[self.ModPath2Index[child]].append(self.ModPath2Index[parent])
                cycle = self.__find_cycle_start_node()
                if cycle is True:
                    print("\033[33mWARNING: \033[0m" + "Module cycle was detected. (" + self.ModPath2ModName[parent] + " --> " + self.ModPath2ModName[child] + ")")
                    self.module_graph[self.ModPath2Index[parent]].remove(self.ModPath2Index[child])
                    self.indegree[self.ModPath2Index[child]] -= 1
                    self.reverse_module_graph[self.ModPath2Index[child]].remove(self.ModPath2Index[parent])

        return 0

    def __find_all_parents_for_loading_module(self, target_module):
        fqueue = [(self.ModPath2Index[self.ModName2ModPath[target_module]])]
        visited = [False] * (self.module_number + 1)
        result = []

        while fqueue:
            node = fqueue.pop(0)

            if self.reverse_module_graph[node] is None:
                continue
            if visited[node] == True:
                result.remove(self.Index2ModPath[node])
                result.append(self.Index2ModPath[node])
                continue
            if self.ModPath2ModName[self.Index2ModPath[node]] == ROOT:
                continue

            visited[node] = True

            result.append(self.Index2ModPath[node])

            for child in self.reverse_module_graph[node]:
                if child is None:
                    continue

                fqueue.append((child))

        return result

    def __remove_module(self, module_path):
        mod_index = self.ModPath2Index[module_path]
        if self.module_graph[mod_index] is not None:
            for edge_list in self.module_graph:
                if edge_list is None:
                    continue

                if mod_index in edge_list:
                    edge_list.remove(mod_index)

            for neighbor_index in self.module_graph[mod_index]:
                if self.indegree[neighbor_index] is None:
                    continue
                self.indegree[neighbor_index] -= 1
                self.reverse_module_graph[neighbor_index].remove(mod_index)

                if self.indegree[neighbor_index] == 0:
                    self.module_graph[0].append(neighbor_index)
                    self.reverse_module_graph[neighbor_index].append(0)
                    self.indegree[neighbor_index] += 1

            self.module_graph[mod_index] = None
            self.reverse_module_graph[mod_index] = None
            self.indegree[mod_index] = None
        return 0

    def add_vendor_boot_modules(self, vendor_boot_modules):
        for module_path in vendor_boot_modules.readlines():
            module_name = module_path.split('/')[-1].rsplit('.', 1)[0]
            self.first_stage_module_list.append(module_name)

        return 0

    def __update_first_stage_modules(self):
        for module_name in self.first_stage_module_list:
            module_list = self.__find_all_parents_for_loading_module(module_name)
            module_list.reverse()

            requirable_module_list = []
            for module_path in module_list:

                if self.ModPath2ModName[module_path] not in self.first_stage_module_list:
                    requirable_module_list.append(self.ModPath2ModName[module_path])

                removed_kernel_module_path = module_path.replace("kernel/", "", 1)
                self.reordered_module_list.append("    \"" + removed_kernel_module_path + "\",")
                self.__remove_module(module_path)

            if requirable_module_list:
                print("\033[91mERROR: \033[0m" + "\"" + module_name + "\" need to some module")
                print("\033[91mERROR: \033[0m" + "This is a list of modules required by" + "\"" + module_name + "\"")
                print("------------------------------------------------------")
                for rmod in requirable_module_list:
                    print(rmod)
                print("------------------------------------------------------")
                print("\033[91mERROR: \033[0m" + "these modules were not in vendor_boot ramdisk(but \"" + module_name + "\" was in vendor_boot ramdisk)")
                print("\033[91mERROR: \033[0m" + "So these modules need to be moved to vendor_boot list")
                sys.exit(-1)

        return 0

    def __update_reordered_module_list(self):
        queue=deque()
        queue.append(self.ModPath2Index[ROOT])
        copy_indgree = self.indegree.copy()
        result = []

        while queue:
            target = queue.popleft()

            result.append(target)

            for voltex in self.module_graph[target]:
                if copy_indgree[voltex] is None:
                    continue

                copy_indgree[voltex]-=1

                if copy_indgree[voltex] == 0:
                    queue.append(voltex)

        for i in result:
            str = self.Index2ModPath[i]
            if str == ROOT:
                continue

            removed_kernel_str = str.replace("kernel/", "", 1)
            self.insmod_module_list.append("    \"" + removed_kernel_str + "\",")
        return 0
    def add_system_dlkm_modules(self, system_dlkm_module_load):
        self.system_dlkm_module_list = system_dlkm_module_load.readlines()

    def __remove_system_dlkm_modules(self):
        for sdlkm_module in self.system_dlkm_module_list:
            sdlkm_module = sdlkm_module.strip()
            mod_name = sdlkm_module.split('/')[-1].rsplit('.', 1)[0]
            self.__remove_module(self.ModName2ModPath[mod_name])

        return 0
    def relocate_all_modules(self, first_stage_module_file, insmod_module_file):
        self.__remove_system_dlkm_modules()
        self.__update_first_stage_modules()
        self.__update_reordered_module_list()

        for line in self.reordered_module_list:
            first_stage_module_file.write(line + "\n")
        for line in self.insmod_module_list:
            insmod_module_file.write(line + "\n")

        return 0

def main(
    vendor_boot_modules: TextIO,
    vendor_dlkm_modules: TextIO,
    system_dlkm_modules: TextIO,
    modules_dep: TextIO,
    modules_alias: TextIO,
    out_vendor_boot_modules_list: TextIO,
    out_vendor_dlkm_modules_list: TextIO,
    fw_devlink: TextIO
):
    Modules = KernelModulesGraph(modules_dep)

    if Modules.insert_fw_devlink(fw_devlink, modules_alias):
        print("\033[33mWARNING: \033[0m" + "Can't parse fw_devlink, so skip firmware devlink")

    Modules.create_module_graph()

    Modules.add_system_dlkm_modules(system_dlkm_modules)

    Modules.add_vendor_boot_modules(vendor_boot_modules)

    Modules.relocate_all_modules(out_vendor_boot_modules_list, out_vendor_dlkm_modules_list)

    return

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vendor_boot_modules", type=argparse.FileType("r"))
    parser.add_argument("--vendor_dlkm_modules", type=argparse.FileType("r"))
    parser.add_argument("--system_dlkm_modules", type=argparse.FileType("r"))
    parser.add_argument("--modules_dep", type=argparse.FileType("r"))
    parser.add_argument("--modules_alias", type=argparse.FileType("r"))
    parser.add_argument("--out_vendor_boot_modules_list", type=argparse.FileType("w+"))
    parser.add_argument("--out_vendor_dlkm_modules_list", type=argparse.FileType("w+"))
    parser.add_argument("--fw_devlink", type=argparse.FileType("r"))
    args = parser.parse_args()
    main(**vars(args))
