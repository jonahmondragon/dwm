1. Apply the restoreafterrestart patch
2. Apply the single_tagset patch.
3. Modify the new tag behavior after single_tagset to simply move all of the
   tags from one monitor to another (this probably means that the single_tagset
   is actually not necessary). The work has started with the transfermon()
   function.
