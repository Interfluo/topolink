smoother is an elliptic solver. The smoother first runs on topo edges belonging to an edge group, it projects those nodes onto the underlying geom edge group and smooths the placement of the edges. Once the edges are smoothed, it runs on the face smoother. The face smoother solves for face nodes and edges not constrained to an underlying geom edge group. 

The smoother runs a projection based algorithm where node are smoother and then reprojected at each iteration. 

Boundary conditions for the smoother include fixed placement (like some edge nodes are fixed after edge smoothing), fixed spacing (may implement way to set spacing off of topo edge groups) or free floating. 