
#
# deliver files from a file list in order via SAM
#

class deliverer:

    def __init__(self, file_list_file):
        self.ds = `date +%Y-%m-%d-%H:%M:%S`
        self.bunch_size = 20
        self.project_prefix= "ifdh_order_$USER_$ds__"
        self.file_list_file = file_list_file
        self.ifdh = ifdh.ifdh()
        self.curbatch = 0
        self.file_list = []

    def make_datasets(self):
	count = 0
	batch = 0
	not_eof = True
        fl = open(self.file_list_file, "r")
	while not_eof:
	    batch = batch + 1
	    count = 0
	    while count < bunch_size:
		count=count + 1
                file = fl.readline()
                if not file:
		    not_eof = False
                    break
                file = os.path.basename(path)
                self.file_list.append(file)
            dims = "file_name " + file_list.join(" file_name ")
            dsname = "%s%d"%(self.project_prefix,batch) 
            # xxx check args
	    self.ifdh.defineDataset(dsname, dims,...)
	    filelist=""
        fl.close()
        self.batches = batch

    def start_projects():
        for i in range(0,self.batches):
            # xxx check args
            self.projects[i] = ifdh.startProject("%s%d"%(self.project_prefix,i),...)

    def get_another_file(self):
	uri = ifdh.getNextFile(self.projects[self.curbatch])
        if not uri:
            ifdh.endProject(self.projects[self.curbatch])
            self.curbatch = self.curbatch+1
            if self.curbatch < self.batches:
	        uri = ifdh.getNextFile(self.projects[self.curbatch])
        if uri:
	    return ifdh.fetchInput(uri)
   
    def apply_command(self,command):

        self.make_datasets()
        self.start_projects()
           
        for file in self.filelist:
             local = self.ifdh.locateFile(file)
             count = 0
             while 0 != os.access(local, os.ROK) && count < self.batches:
                 count = count + 1
                 self.get_another_file()
             command(local)

if __name__ == "__MAIN__":

    def echo(s): print s

    d = deliverer(sys.argv[1])
    d.apply_command(echo)
